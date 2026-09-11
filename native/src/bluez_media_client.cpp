// bluez_media_client.cpp — C ABI entry points for BlueZ Media registration.

#include "bluez_media_native.h"

#include <sdbus-c++/sdbus-c++.h>

#include <dlfcn.h>
#include <unistd.h>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <unordered_map>

#include "bluez_media_types.h"
#include "cover_art_service.h"
#include "media_browser_proxy.h"
#include "media_client.h"
#include "media_control_proxy.h"
#include "media_object_manager.h"
#include "media_player_proxy.h"
#include "media_transport_proxy.h"

class OperationQueue {
 public:
  OperationQueue() : worker_([this]() { run(); }) {}
  ~OperationQueue() { stop(); }

  OperationQueue(const OperationQueue&) = delete;
  OperationQueue& operator=(const OperationQueue&) = delete;

  void post(std::function<void()> operation) {
    {
      const std::scoped_lock lock(mutex_);
      if (stopping_)
        return;
      operations_.push_back(std::move(operation));
    }
    ready_.notify_one();
  }

  void stop() {
    {
      const std::scoped_lock lock(mutex_);
      stopping_ = true;
    }
    ready_.notify_one();
    if (worker_.joinable()) {
      worker_.join();
    }
  }

 private:
  void run() {
    while (true) {
      std::function<void()> operation;
      {
        std::unique_lock lock(mutex_);
        ready_.wait(lock,
                    [this]() { return stopping_ || !operations_.empty(); });
        if (operations_.empty()) {
          return;
        }
        operation = std::move(operations_.front());
        operations_.pop_front();
      }
      operation();
    }
  }

  std::mutex mutex_;
  std::condition_variable ready_;
  std::deque<std::function<void()>> operations_;
  bool stopping_ = false;
  std::thread worker_;
};

struct BluezMediaClientContext;
namespace {
void release_pending_fds(BluezMediaClientContext& context);
}

struct BluezMediaClientContext {
  std::unique_ptr<sdbus::IConnection> conn;
  std::unique_ptr<MediaClient> client;
  std::unique_ptr<CoverArtService> cover_art;
  std::unique_ptr<MediaObjectManager> object_manager;
  OperationQueue operations;
  OperationQueue cover_operations;
  bool event_loop_started = false;
  std::set<uintptr_t> pending_fds;

  ~BluezMediaClientContext() {
    operations.stop();
    cover_operations.stop();
    release_pending_fds(*this);
    cover_art.reset();
    client.reset();
    if (event_loop_started) {
      conn->leaveEventLoop();
    }
  }
};

namespace {

struct ClientRegistry {
  // Declared first so outstanding cleanup is joined after the map is destroyed.
  OperationQueue reaper;
  OperationQueue connections;
  std::mutex mutex;
  std::unordered_map<uintptr_t, std::shared_ptr<BluezMediaClientContext>>
      clients;
  uintptr_t next_handle = 1;
  std::unordered_map<uintptr_t, int> fds;
  uintptr_t next_fd_token = 1;

  ~ClientRegistry() {
    connections.stop();
    reaper.stop();
    clients.clear();
    for (const auto& [token, fd] : fds)
      ::close(fd);
    fds.clear();
  }
};

ClientRegistry& registry() {
  // Variants depend on sdbus-c++'s process-wide pseudo connection. Construct it
  // before the registry so all cached variants are destroyed before it.
  static const sdbus::Variant lifetime_anchor;
  static ClientRegistry instance;
  return instance;
}

void release_pending_fds(BluezMediaClientContext& context) {
  auto& state = registry();
  const std::scoped_lock lock(state.mutex);
  for (const auto token : context.pending_fds) {
    const auto entry = state.fds.find(token);
    if (entry != state.fds.end()) {
      ::close(entry->second);
      state.fds.erase(entry);
    }
  }
  context.pending_fds.clear();
}

uintptr_t retain_fd(BluezMediaClientContext& context, sdbus::UnixFd& fd) {
  auto& state = registry();
  const std::scoped_lock lock(state.mutex);
  const auto token = state.next_fd_token++;
  context.pending_fds.insert(token);
  state.fds.emplace(token, fd.get());
  fd.release();
  return token;
}

void* register_context(std::shared_ptr<BluezMediaClientContext> context) {
  auto& state = registry();
  const std::scoped_lock lock(state.mutex);
  const auto token = state.next_handle++;
  state.clients.emplace(token, std::move(context));
  return reinterpret_cast<void*>(token);
}

std::shared_ptr<BluezMediaClientContext> get_context(void* handle) {
  auto& state = registry();
  const std::scoped_lock lock(state.mutex);
  const auto context = state.clients.find(reinterpret_cast<uintptr_t>(handle));
  return context == state.clients.end() ? nullptr : context->second;
}

std::shared_ptr<BluezMediaClientContext> retire_context(void* handle) {
  auto& state = registry();
  const std::scoped_lock lock(state.mutex);
  const auto context = state.clients.find(reinterpret_cast<uintptr_t>(handle));
  if (context == state.clients.end())
    return nullptr;
  auto result = std::move(context->second);
  state.clients.erase(context);
  return result;
}

bool is_invalid_media_player_parameter(const sdbus::Error& error) {
  return error.getName() == "org.bluez.Error.Failed" &&
         error.getMessage().find("Invalid Parameter") != std::string::npos;
}

void log_exception(const char* function_name, const std::exception& e) {
  fprintf(stderr, "%s: %s\n", function_name, e.what());
}

int write_payload(const std::vector<uint8_t>& payload, BluezMediaBuffer* out) {
  if (out == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  out->data = nullptr;
  out->length = 0;
  if (payload.empty() ||
      payload.size() >
          static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  out->data = static_cast<uint8_t*>(std::malloc(payload.size()));
  if (out->data == nullptr) {
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
  std::memcpy(out->data, payload.data(), payload.size());
  out->length = static_cast<int32_t>(payload.size());
  return BLUEZ_MEDIA_SUCCESS;
}

bool prepare_buffer(BluezMediaBuffer* out) {
  if (out == nullptr) {
    return false;
  }
  out->data = nullptr;
  out->length = 0;
  return true;
}

std::shared_ptr<BluezMediaClientContext> create_context(
    int64_t events_port,
    bool manage_cover_art = true) {
  auto ctx = std::make_shared<BluezMediaClientContext>();
  ctx->conn = sdbus::createSystemBusConnection();
  ctx->client = std::make_unique<MediaClient>(*ctx->conn);
  auto* context = ctx.get();
  ctx->cover_art = std::make_unique<CoverArtService>(*ctx->conn, [context]() {
    context->cover_operations.post([context]() {
      try {
        context->cover_art->reconnect_players(std::chrono::seconds{15});
      } catch (const std::exception& error) {
        log_exception("bluez_media cover art reconnect", error);
      } catch (...) {
        fprintf(stderr, "bluez_media cover art reconnect: unknown exception\n");
      }
    });
  });
  ctx->object_manager = std::make_unique<MediaObjectManager>(
      *ctx->conn, events_port,
      [context](const std::string& path, const std::string& interface_name) {
        context->operations.post([context, path, interface_name]() {
          if (interface_name == "org.bluez.Media1") {
            context->client->invalidate_registrations(path);
            context->cover_operations.post(
                [context]() { context->cover_art->reset(); });
          } else if (interface_name == "org.bluez.MediaPlayer1") {
            context->cover_operations.post([context, path]() {
              context->cover_art->unregister_player(path);
            });
          }
        });
      },
      [context, manage_cover_art](const std::string& path,
                                  const sdbus::ObjectPath& device_path,
                                  uint16_t obex_port) {
        if (!manage_cover_art)
          return;
        context->cover_operations.post([context, path, device_path,
                                        obex_port]() {
          try {
            context->cover_art->register_player(
                path, device_path, obex_port,
                std::chrono::steady_clock::now() + std::chrono::seconds{15});
          } catch (const std::exception& error) {
            log_exception("bluez_media cover art session", error);
          } catch (...) {
            fprintf(stderr,
                    "bluez_media cover art session: unknown exception\n");
          }
        });
      });
  ctx->object_manager->get_managed_objects();
  ctx->conn->enterEventLoopAsync();
  ctx->event_loop_started = true;
  return ctx;
}

bool post_bytes(int64_t port,
                uint8_t tag,
                const std::vector<uint8_t>& payload = {}) {
  std::vector<uint8_t> message;
  message.reserve(payload.size() + 1);
  message.push_back(tag);
  message.insert(message.end(), payload.begin(), payload.end());

  Dart_CObject object;
  object.type = Dart_CObject_kTypedData;
  object.value.as_typed_data.type = Dart_TypedData_kUint8;
  object.value.as_typed_data.length = static_cast<intptr_t>(message.size());
  object.value.as_typed_data.values = message.data();
  return Dart_PostCObject_DL(port, &object);
}

void post_error(int64_t port,
                const std::string& object_path,
                const std::string& name,
                const std::string& message) noexcept {
  try {
    post_bytes(port, 0x20,
               glz::encode(BlueZMediaError{object_path, name, message}));
  } catch (...) {
  }
}

void post_status(int64_t port,
                 const std::string& object_path,
                 int status) noexcept {
  try {
    if (status == BLUEZ_MEDIA_SUCCESS) {
      post_bytes(port, 0xFF);
      return;
    }

    const char* name = "org.bluez.Error.Failed";
    const char* message = "BlueZ media operation failed";
    switch (status) {
      case BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT:
        name = "org.bluez.Error.InvalidArguments";
        message = "Invalid media operation argument or retired client handle";
        break;
      case BLUEZ_MEDIA_ERROR_UNSUPPORTED_SETTING:
        name = "org.bluez.Error.NotSupported";
        message = "The media setting is not supported";
        break;
      case BLUEZ_MEDIA_ERROR_ALREADY_EXISTS:
        name = "org.bluez.Error.AlreadyExists";
        message = "The media object is already registered";
        break;
      case BLUEZ_MEDIA_ERROR_NOT_FOUND:
        name = "org.bluez.Error.DoesNotExist";
        message = "The media object was not found";
        break;
      default:
        break;
    }
    post_error(port, object_path, name, message);
  } catch (...) {
  }
}

void dispatch_async(const std::shared_ptr<BluezMediaClientContext>& ctx,
                    BluezMediaOperation operation,
                    std::string object_path,
                    std::string argument,
                    int32_t value,
                    int64_t result_port) {
  auto* context = ctx.get();
  const bool cover_operation =
      operation == BLUEZ_MEDIA_OP_PLAYER_GET_COVER_ART ||
      operation == BLUEZ_MEDIA_OP_ITEM_GET_COVER_ART ||
      operation == BLUEZ_MEDIA_OP_PLAYER_GET_COVER_ART_FROM_EXISTING_SESSION ||
      operation == BLUEZ_MEDIA_OP_ITEM_GET_COVER_ART_FROM_EXISTING_SESSION;
  auto& queue = cover_operation ? ctx->cover_operations : ctx->operations;
  queue.post([context, operation, object_path = std::move(object_path),
              argument = std::move(argument), value, result_port]() {
    try {
      int status = BLUEZ_MEDIA_SUCCESS;
      std::vector<uint8_t> payload;
      std::optional<sdbus::UnixFd> acquired_fd;

      switch (operation) {
        case BLUEZ_MEDIA_OP_PLAYER_PLAY:
        case BLUEZ_MEDIA_OP_PLAYER_PAUSE:
        case BLUEZ_MEDIA_OP_PLAYER_STOP:
        case BLUEZ_MEDIA_OP_PLAYER_NEXT:
        case BLUEZ_MEDIA_OP_PLAYER_PREVIOUS:
        case BLUEZ_MEDIA_OP_PLAYER_FAST_FORWARD:
        case BLUEZ_MEDIA_OP_PLAYER_REWIND:
        case BLUEZ_MEDIA_OP_PLAYER_SET_REPEAT:
        case BLUEZ_MEDIA_OP_PLAYER_SET_SHUFFLE:
        case BLUEZ_MEDIA_OP_PLAYER_GET_PROPERTIES: {
          MediaPlayerProxy proxy{*context->conn, object_path};
          switch (operation) {
            case BLUEZ_MEDIA_OP_PLAYER_PLAY:
              status = proxy.play();
              break;
            case BLUEZ_MEDIA_OP_PLAYER_PAUSE:
              status = proxy.pause();
              break;
            case BLUEZ_MEDIA_OP_PLAYER_STOP:
              status = proxy.stop();
              break;
            case BLUEZ_MEDIA_OP_PLAYER_NEXT:
              status = proxy.next();
              break;
            case BLUEZ_MEDIA_OP_PLAYER_PREVIOUS:
              status = proxy.previous();
              break;
            case BLUEZ_MEDIA_OP_PLAYER_FAST_FORWARD:
              status = proxy.fast_forward();
              break;
            case BLUEZ_MEDIA_OP_PLAYER_REWIND:
              status = proxy.rewind();
              break;
            case BLUEZ_MEDIA_OP_PLAYER_SET_REPEAT:
              status = proxy.set_repeat(argument);
              break;
            case BLUEZ_MEDIA_OP_PLAYER_SET_SHUFFLE:
              status = proxy.set_shuffle(argument);
              break;
            case BLUEZ_MEDIA_OP_PLAYER_GET_PROPERTIES:
              payload = proxy.properties();
              break;
            default:
              break;
          }
          break;
        }
        case BLUEZ_MEDIA_OP_PLAYER_GET_COVER_ART:
          status = context->cover_art->get(object_path, argument,
                                           std::chrono::milliseconds{value});
          break;
        case BLUEZ_MEDIA_OP_ITEM_GET_COVER_ART:
          status = context->cover_art->get_item(
              object_path, argument, std::chrono::milliseconds{value});
          break;
        case BLUEZ_MEDIA_OP_PLAYER_GET_COVER_ART_FROM_EXISTING_SESSION:
          status = context->cover_art->get_from_existing_session(
              object_path, argument, std::chrono::milliseconds{value});
          break;
        case BLUEZ_MEDIA_OP_ITEM_GET_COVER_ART_FROM_EXISTING_SESSION:
          status = context->cover_art->get_item_from_existing_session(
              object_path, argument, std::chrono::milliseconds{value});
          break;
        case BLUEZ_MEDIA_OP_CONTROL_PLAY:
        case BLUEZ_MEDIA_OP_CONTROL_PAUSE:
        case BLUEZ_MEDIA_OP_CONTROL_STOP:
        case BLUEZ_MEDIA_OP_CONTROL_NEXT:
        case BLUEZ_MEDIA_OP_CONTROL_PREVIOUS:
        case BLUEZ_MEDIA_OP_CONTROL_VOLUME_UP:
        case BLUEZ_MEDIA_OP_CONTROL_VOLUME_DOWN:
        case BLUEZ_MEDIA_OP_CONTROL_FAST_FORWARD:
        case BLUEZ_MEDIA_OP_CONTROL_REWIND:
        case BLUEZ_MEDIA_OP_CONTROL_GET_PROPERTIES: {
          MediaControlProxy proxy{*context->conn, object_path};
          switch (operation) {
            case BLUEZ_MEDIA_OP_CONTROL_PLAY:
              status = proxy.play();
              break;
            case BLUEZ_MEDIA_OP_CONTROL_PAUSE:
              status = proxy.pause();
              break;
            case BLUEZ_MEDIA_OP_CONTROL_STOP:
              status = proxy.stop();
              break;
            case BLUEZ_MEDIA_OP_CONTROL_NEXT:
              status = proxy.next();
              break;
            case BLUEZ_MEDIA_OP_CONTROL_PREVIOUS:
              status = proxy.previous();
              break;
            case BLUEZ_MEDIA_OP_CONTROL_VOLUME_UP:
              status = proxy.volume_up();
              break;
            case BLUEZ_MEDIA_OP_CONTROL_VOLUME_DOWN:
              status = proxy.volume_down();
              break;
            case BLUEZ_MEDIA_OP_CONTROL_FAST_FORWARD:
              status = proxy.fast_forward();
              break;
            case BLUEZ_MEDIA_OP_CONTROL_REWIND:
              status = proxy.rewind();
              break;
            case BLUEZ_MEDIA_OP_CONTROL_GET_PROPERTIES:
              payload = proxy.properties();
              break;
            default:
              break;
          }
          break;
        }
        case BLUEZ_MEDIA_OP_FOLDER_SEARCH:
        case BLUEZ_MEDIA_OP_FOLDER_LIST_ITEMS:
        case BLUEZ_MEDIA_OP_FOLDER_CHANGE_FOLDER:
        case BLUEZ_MEDIA_OP_FOLDER_GET_PROPERTIES:
        case BLUEZ_MEDIA_OP_ITEM_PLAY:
        case BLUEZ_MEDIA_OP_ITEM_ADD_TO_NOW_PLAYING:
        case BLUEZ_MEDIA_OP_ITEM_GET_PROPERTIES: {
          MediaBrowserProxy proxy{*context->conn};
          switch (operation) {
            case BLUEZ_MEDIA_OP_FOLDER_SEARCH:
              payload = proxy.folder_search(object_path, argument);
              break;
            case BLUEZ_MEDIA_OP_FOLDER_LIST_ITEMS:
              payload = proxy.folder_list_items(object_path);
              break;
            case BLUEZ_MEDIA_OP_FOLDER_CHANGE_FOLDER:
              status = proxy.folder_change_folder(object_path, argument);
              break;
            case BLUEZ_MEDIA_OP_FOLDER_GET_PROPERTIES:
              payload = proxy.folder_properties(object_path);
              break;
            case BLUEZ_MEDIA_OP_ITEM_PLAY:
              status = proxy.item_play(object_path);
              break;
            case BLUEZ_MEDIA_OP_ITEM_ADD_TO_NOW_PLAYING:
              status = proxy.item_add_to_now_playing(object_path);
              break;
            case BLUEZ_MEDIA_OP_ITEM_GET_PROPERTIES:
              payload = proxy.item_properties(object_path);
              break;
            default:
              break;
          }
          break;
        }
        case BLUEZ_MEDIA_OP_TRANSPORT_ACQUIRE:
        case BLUEZ_MEDIA_OP_TRANSPORT_TRY_ACQUIRE:
        case BLUEZ_MEDIA_OP_TRANSPORT_RELEASE:
        case BLUEZ_MEDIA_OP_TRANSPORT_GET_PROPERTIES:
        case BLUEZ_MEDIA_OP_TRANSPORT_SET_VOLUME: {
          MediaTransportProxy proxy{*context->conn, object_path};
          switch (operation) {
            case BLUEZ_MEDIA_OP_TRANSPORT_ACQUIRE: {
              // Guard the raw fd with UnixFd so it is closed if glz::encode
              // throws (e.g. OOM), preventing a file-descriptor leak.
              auto result = proxy.acquire();
              acquired_fd.emplace(result.fd, sdbus::adopt_fd);
              payload = glz::encode(result);
              break;
            }
            case BLUEZ_MEDIA_OP_TRANSPORT_TRY_ACQUIRE: {
              auto result = proxy.try_acquire();
              acquired_fd.emplace(result.fd, sdbus::adopt_fd);
              payload = glz::encode(result);
              break;
            }
            case BLUEZ_MEDIA_OP_TRANSPORT_RELEASE:
              status = proxy.release();
              break;
            case BLUEZ_MEDIA_OP_TRANSPORT_GET_PROPERTIES:
              payload = proxy.properties();
              break;
            case BLUEZ_MEDIA_OP_TRANSPORT_SET_VOLUME:
              status = proxy.set_volume(value);
              break;
            default:
              break;
          }
          break;
        }
        case BLUEZ_MEDIA_OP_GET_MANAGED_OBJECTS:
          payload = context->client->get_managed_objects();
          break;
        case BLUEZ_MEDIA_OP_UNREGISTER_PLAYER:
          status = context->client->unregister_player(object_path.c_str(),
                                                      argument.c_str());
          break;
        default:
          status = BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
          break;
      }

      if (acquired_fd) {
        const auto token = retain_fd(*context, *acquired_fd);
        try {
          std::vector<uint8_t> owned_payload;
          glz::detail::write_integer(owned_payload,
                                     static_cast<uint64_t>(token));
          owned_payload.insert(owned_payload.end(), payload.begin(),
                               payload.end());
          // Ownership stays native until Dart has installed its finalizer and
          // claimed this token. Closing an unread port cannot leak the fd.
          if (!post_bytes(result_port, 0x11, owned_payload))
            bluez_media_release_fd_token(reinterpret_cast<void*>(token));
        } catch (...) {
          bluez_media_release_fd_token(reinterpret_cast<void*>(token));
          throw;
        }
      } else if (status != BLUEZ_MEDIA_SUCCESS) {
        post_status(result_port, object_path, status);
      } else {
        post_bytes(result_port, payload.empty() ? 0xFF : 0x10, payload);
      }
    } catch (const sdbus::Error& error) {
      post_error(result_port, object_path, error.getName(), error.getMessage());
    } catch (const std::exception& error) {
      post_error(result_port, object_path, "org.bluez.Error.Failed",
                 error.what());
    } catch (...) {
      post_error(result_port, object_path, "org.bluez.Error.Failed",
                 "Unknown C++ exception");
    }
  });
}

}  // namespace

extern "C" {

const char* bluez_media_library_path(void) {
  Dl_info info{};
  if (dladdr(reinterpret_cast<void*>(&bluez_media_library_path), &info) == 0)
    return nullptr;
  return info.dli_fname;
}

void bluez_media_init(void* dart_api_dl_data) {
  Dart_InitializeApiDL(dart_api_dl_data);
}

void* bluez_media_client_create(int64_t events_port) {
  try {
    return register_context(create_context(events_port));
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_client_create: %s\n", e.what());
    return nullptr;
  } catch (const std::exception& e) {
    log_exception("bluez_media_client_create", e);
    return nullptr;
  } catch (...) {
    fprintf(stderr, "bluez_media_client_create: unknown C++ exception\n");
    return nullptr;
  }
}

void bluez_media_client_create_async(int64_t events_port, int64_t result_port) {
  bluez_media_client_create_with_options_async(events_port, result_port, 1);
}

void bluez_media_client_create_with_options_async(int64_t events_port,
                                                  int64_t result_port,
                                                  uint8_t manage_cover_art) {
  try {
    registry().connections.post([events_port, result_port, manage_cover_art]() {
      try {
        void* handle = register_context(
            create_context(events_port, manage_cover_art != 0));
        Dart_CObject result;
        result.type = Dart_CObject_kInt64;
        result.value.as_int64 =
            static_cast<int64_t>(reinterpret_cast<uintptr_t>(handle));
        if (!Dart_PostCObject_DL(result_port, &result)) {
          retire_context(handle);
        }
      } catch (const sdbus::Error& error) {
        post_error(result_port, "", error.getName(), error.getMessage());
      } catch (const std::exception& error) {
        post_error(result_port, "", "org.bluez.Error.Failed", error.what());
      } catch (...) {
        post_error(result_port, "", "org.bluez.Error.Failed",
                   "Unknown C++ exception");
      }
    });
  } catch (const std::exception& error) {
    post_error(result_port, "", "org.bluez.Error.Failed", error.what());
  } catch (...) {
    post_error(result_port, "", "org.bluez.Error.Failed",
               "Unknown C++ exception");
  }
}

void bluez_media_client_destroy(void* handle) {
  try {
    auto ctx = retire_context(handle);
    if (!ctx) {
      return;
    }

    // A D-Bus call already queued for this client may still be waiting for its
    // reply. Reap it away from the Dart isolate so close() never inherits that
    // wait; retiring the token above still rejects every new call immediately.
    // Note: BluezMediaClientContext::~BluezMediaClientContext already calls
    // operations.stop(), so we must not call it again here.
    registry().reaper.post([ctx = std::move(ctx)]() mutable { ctx.reset(); });
  } catch (...) {
    // The retired context is destroyed locally if the reaper cannot start.
  }
}

void bluez_media_client_destroy_async(void* handle, int64_t result_port) {
  try {
    auto ctx = retire_context(handle);
    registry().reaper.post([ctx = std::move(ctx), result_port]() mutable {
      ctx.reset();
      post_status(result_port, "", BLUEZ_MEDIA_SUCCESS);
    });
  } catch (const std::exception& error) {
    post_error(result_port, "", "org.bluez.Error.Failed", error.what());
  } catch (...) {
    post_error(result_port, "", "org.bluez.Error.Failed",
               "Native shutdown failed");
  }
}

void bluez_media_buffer_free(BluezMediaBuffer* buffer) {
  if (buffer == nullptr) {
    return;
  }
  std::free(buffer->data);
  buffer->data = nullptr;
  buffer->length = 0;
}

void bluez_media_call_async(void* handle,
                            BluezMediaOperation operation,
                            const char* object_path,
                            const char* argument,
                            int32_t value,
                            int64_t result_port) {
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      post_status(result_port, object_path == nullptr ? "" : object_path,
                  BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT);
      return;
    }
    dispatch_async(ctx, operation, object_path == nullptr ? "" : object_path,
                   argument == nullptr ? "" : argument, value, result_port);
  } catch (const std::exception& error) {
    post_error(result_port, object_path == nullptr ? "" : object_path,
               "org.bluez.Error.Failed", error.what());
  } catch (...) {
    post_error(result_port, object_path == nullptr ? "" : object_path,
               "org.bluez.Error.Failed", "Unknown C++ exception");
  }
}

int bluez_media_register_player(
    void* handle,
    const BluezMediaPlayerRegistration* registration) {
  if (handle == nullptr || registration == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }

  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    return ctx->client->register_player(*registration);
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_register_player: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& e) {
    log_exception("bluez_media_register_player", e);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

void bluez_media_register_player_async(
    void* handle,
    const BluezMediaPlayerRegistration* registration,
    int64_t result_port) {
  try {
    const auto ctx = get_context(handle);
    if (!ctx || registration == nullptr ||
        registration->adapter_path == nullptr ||
        registration->player_path == nullptr || registration->name == nullptr ||
        registration->type == nullptr || registration->subtype == nullptr) {
      post_status(result_port, "", BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT);
      return;
    }

    const std::string adapter_path{registration->adapter_path};
    const std::string player_path{registration->player_path};
    const std::string name{registration->name};
    const std::string type{registration->type};
    const std::string subtype{registration->subtype};
    const uint8_t browsable = registration->browsable;
    const uint8_t searchable = registration->searchable;

    auto* context = ctx.get();
    ctx->operations.post([context, adapter_path, player_path, name, type,
                          subtype, browsable, searchable, result_port]() {
      BluezMediaPlayerRegistration owned{
          adapter_path.c_str(), player_path.c_str(), name.c_str(), type.c_str(),
          subtype.c_str(),      browsable,           searchable};
      try {
        post_status(result_port, player_path,
                    context->client->register_player(owned));
      } catch (const sdbus::Error& error) {
        post_error(result_port, player_path, error.getName(),
                   error.getMessage());
      } catch (const std::exception& error) {
        post_error(result_port, player_path, "org.bluez.Error.Failed",
                   error.what());
      } catch (...) {
        post_error(result_port, player_path, "org.bluez.Error.Failed",
                   "Unknown C++ exception");
      }
    });
  } catch (const std::exception& error) {
    post_error(result_port, "", "org.bluez.Error.Failed", error.what());
  } catch (...) {
    post_error(result_port, "", "org.bluez.Error.Failed",
               "Unknown C++ exception");
  }
}

int bluez_media_unregister_player(void* handle,
                                  const char* adapter_path,
                                  const char* player_path) {
  if (handle == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }

  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    return ctx->client->unregister_player(adapter_path, player_path);
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_unregister_player: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_player_play(void* handle, const char* player_path) {
  if (handle == nullptr || player_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return proxy.play();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_player_play: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_player_pause(void* handle, const char* player_path) {
  if (handle == nullptr || player_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return proxy.pause();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_player_pause: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_player_stop(void* handle, const char* player_path) {
  if (handle == nullptr || player_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return proxy.stop();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_player_stop: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_player_next(void* handle, const char* player_path) {
  if (handle == nullptr || player_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return proxy.next();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_player_next: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_player_previous(void* handle, const char* player_path) {
  if (handle == nullptr || player_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return proxy.previous();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_player_previous: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_player_fast_forward(void* handle, const char* player_path) {
  if (handle == nullptr || player_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return proxy.fast_forward();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_player_fast_forward: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_player_rewind(void* handle, const char* player_path) {
  if (handle == nullptr || player_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return proxy.rewind();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_player_rewind: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_player_set_repeat(void* handle,
                                  const char* player_path,
                                  const char* repeat) {
  if (handle == nullptr || player_path == nullptr || repeat == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return proxy.set_repeat(repeat);
  } catch (const sdbus::Error& e) {
    if (is_invalid_media_player_parameter(e)) {
      return BLUEZ_MEDIA_ERROR_UNSUPPORTED_SETTING;
    }
    fprintf(stderr, "bluez_media_player_set_repeat: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_player_set_shuffle(void* handle,
                                   const char* player_path,
                                   const char* shuffle) {
  if (handle == nullptr || player_path == nullptr || shuffle == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return proxy.set_shuffle(shuffle);
  } catch (const sdbus::Error& e) {
    if (is_invalid_media_player_parameter(e)) {
      return BLUEZ_MEDIA_ERROR_UNSUPPORTED_SETTING;
    }
    fprintf(stderr, "bluez_media_player_set_shuffle: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_player_get_properties(void* handle,
                                      const char* player_path,
                                      BluezMediaBuffer* out) {
  if (!prepare_buffer(out) || handle == nullptr || player_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }

  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return write_payload(proxy.properties(), out);
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_player_get_properties: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_player_get_cover_art(void* handle,
                                     const char* player_path,
                                     const char* target_file,
                                     int32_t timeout_ms) {
  if (handle == nullptr || player_path == nullptr || target_file == nullptr ||
      timeout_ms <= 0) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    return ctx->cover_art->get(player_path, target_file,
                               std::chrono::milliseconds{timeout_ms});
  } catch (const std::exception& e) {
    log_exception("bluez_media_player_get_cover_art", e);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_player_get_cover_art_from_existing_session(
    void* handle,
    const char* player_path,
    const char* target_file,
    int32_t timeout_ms) {
  if (handle == nullptr || player_path == nullptr || target_file == nullptr ||
      timeout_ms <= 0) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    return ctx->cover_art->get_from_existing_session(
        player_path, target_file, std::chrono::milliseconds{timeout_ms});
  } catch (const std::exception& e) {
    log_exception("bluez_media_player_get_cover_art_from_existing_session", e);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_control_play(void* handle, const char* control_path) {
  if (handle == nullptr || control_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.play();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_control_play: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_control_pause(void* handle, const char* control_path) {
  if (handle == nullptr || control_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.pause();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_control_pause: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_control_stop(void* handle, const char* control_path) {
  if (handle == nullptr || control_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.stop();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_control_stop: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_control_next(void* handle, const char* control_path) {
  if (handle == nullptr || control_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.next();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_control_next: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_control_previous(void* handle, const char* control_path) {
  if (handle == nullptr || control_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.previous();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_control_previous: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_control_volume_up(void* handle, const char* control_path) {
  if (handle == nullptr || control_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.volume_up();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_control_volume_up: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_control_volume_down(void* handle, const char* control_path) {
  if (handle == nullptr || control_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.volume_down();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_control_volume_down: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_control_fast_forward(void* handle, const char* control_path) {
  if (handle == nullptr || control_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.fast_forward();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_control_fast_forward: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_control_rewind(void* handle, const char* control_path) {
  if (handle == nullptr || control_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.rewind();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_control_rewind: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_control_get_properties(void* handle,
                                       const char* control_path,
                                       BluezMediaBuffer* out) {
  if (!prepare_buffer(out) || handle == nullptr || control_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }

  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaControlProxy proxy{*ctx->conn, control_path};
    return write_payload(proxy.properties(), out);
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_control_get_properties: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_folder_search(void* handle,
                              const char* folder_path,
                              const char* value,
                              BluezMediaBuffer* out) {
  if (!prepare_buffer(out) || handle == nullptr || folder_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }

  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaBrowserProxy proxy{*ctx->conn};
    return write_payload(
        proxy.folder_search(folder_path, value != nullptr ? value : ""), out);
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_folder_search: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_folder_list_items(void* handle,
                                  const char* folder_path,
                                  BluezMediaBuffer* out) {
  if (!prepare_buffer(out) || handle == nullptr || folder_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }

  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaBrowserProxy proxy{*ctx->conn};
    return write_payload(proxy.folder_list_items(folder_path), out);
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_folder_list_items: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_folder_change_folder(void* handle,
                                     const char* folder_path,
                                     const char* target_folder_path) {
  if (handle == nullptr || folder_path == nullptr ||
      target_folder_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }

  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaBrowserProxy proxy{*ctx->conn};
    return proxy.folder_change_folder(folder_path, target_folder_path);
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_folder_change_folder: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_folder_get_properties(void* handle,
                                      const char* folder_path,
                                      BluezMediaBuffer* out) {
  if (!prepare_buffer(out) || handle == nullptr || folder_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }

  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaBrowserProxy proxy{*ctx->conn};
    return write_payload(proxy.folder_properties(folder_path), out);
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_folder_get_properties: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_item_play(void* handle, const char* item_path) {
  if (handle == nullptr || item_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }

  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaBrowserProxy proxy{*ctx->conn};
    return proxy.item_play(item_path);
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_item_play: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_item_add_to_now_playing(void* handle, const char* item_path) {
  if (handle == nullptr || item_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }

  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaBrowserProxy proxy{*ctx->conn};
    return proxy.item_add_to_now_playing(item_path);
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_item_add_to_now_playing: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_item_get_properties(void* handle,
                                    const char* item_path,
                                    BluezMediaBuffer* out) {
  if (!prepare_buffer(out) || handle == nullptr || item_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }

  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaBrowserProxy proxy{*ctx->conn};
    return write_payload(proxy.item_properties(item_path), out);
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_item_get_properties: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_item_get_cover_art(void* handle,
                                   const char* item_path,
                                   const char* target_file,
                                   int32_t timeout_ms) {
  if (handle == nullptr || item_path == nullptr || target_file == nullptr ||
      timeout_ms <= 0) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    return ctx->cover_art->get_item(item_path, target_file,
                                    std::chrono::milliseconds{timeout_ms});
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_item_get_cover_art: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_item_get_cover_art_from_existing_session(
    void* handle,
    const char* item_path,
    const char* target_file,
    int32_t timeout_ms) {
  if (handle == nullptr || item_path == nullptr || target_file == nullptr ||
      timeout_ms <= 0) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    return ctx->cover_art->get_item_from_existing_session(
        item_path, target_file, std::chrono::milliseconds{timeout_ms});
  } catch (const sdbus::Error& e) {
    fprintf(stderr,
            "bluez_media_item_get_cover_art_from_existing_session: %s\n",
            e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

// ── org.bluez.MediaTransport1 remote transports ────────────────────────────

int bluez_media_transport_acquire(void* handle,
                                  const char* transport_path,
                                  BluezMediaBuffer* out) {
  if (!prepare_buffer(out) || handle == nullptr || transport_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaTransportProxy proxy{*ctx->conn, transport_path};
    auto result = proxy.acquire();
    sdbus::UnixFd owned_fd{result.fd, sdbus::adopt_fd};
    result.fd = owned_fd.get();
    const int status = write_payload(glz::encode(result), out);
    if (status == BLUEZ_MEDIA_SUCCESS) {
      owned_fd.release();
    } else {
      try {
        proxy.release();
      } catch (const sdbus::Error&) {
      }
    }
    return status;
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_transport_acquire: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& e) {
    log_exception("bluez_media_transport_acquire", e);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_transport_try_acquire(void* handle,
                                      const char* transport_path,
                                      BluezMediaBuffer* out) {
  if (!prepare_buffer(out) || handle == nullptr || transport_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaTransportProxy proxy{*ctx->conn, transport_path};
    auto result = proxy.try_acquire();
    sdbus::UnixFd owned_fd{result.fd, sdbus::adopt_fd};
    result.fd = owned_fd.get();
    const int status = write_payload(glz::encode(result), out);
    if (status == BLUEZ_MEDIA_SUCCESS) {
      owned_fd.release();
    } else {
      try {
        proxy.release();
      } catch (const sdbus::Error&) {
      }
    }
    return status;
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_transport_try_acquire: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& e) {
    log_exception("bluez_media_transport_try_acquire", e);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_transport_release(void* handle, const char* transport_path) {
  if (handle == nullptr || transport_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaTransportProxy proxy{*ctx->conn, transport_path};
    return proxy.release();
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_transport_release: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_transport_get_properties(void* handle,
                                         const char* transport_path,
                                         BluezMediaBuffer* out) {
  if (!prepare_buffer(out) || handle == nullptr || transport_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaTransportProxy proxy{*ctx->conn, transport_path};
    return write_payload(proxy.properties(), out);
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_transport_get_properties: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_transport_set_volume(void* handle,
                                     const char* transport_path,
                                     uint16_t volume) {
  if (handle == nullptr || transport_path == nullptr) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    MediaTransportProxy proxy{*ctx->conn, transport_path};
    return proxy.set_volume(volume);
  } catch (const sdbus::Error& e) {
    fprintf(stderr, "bluez_media_transport_set_volume: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (const std::exception& error) {
    log_exception("bluez_media C API", error);
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_get_managed_objects(void* handle, BluezMediaBuffer* out) {
  if (!prepare_buffer(out) || handle == nullptr)
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  try {
    const auto ctx = get_context(handle);
    if (!ctx) {
      return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
    }
    return write_payload(ctx->client->get_managed_objects(), out);
  } catch (const std::exception& e) {
    fprintf(stderr, "bluez_media_get_managed_objects: %s\n", e.what());
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  } catch (...) {
    fprintf(stderr, "bluez_media C API: unknown C++ exception\n");
    return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
  }
}

int bluez_media_claim_fd(void* handle, uint64_t token) try {
  auto& state = registry();
  const std::scoped_lock lock(state.mutex);
  const auto client = state.clients.find(reinterpret_cast<uintptr_t>(handle));
  if (client == state.clients.end() || !state.fds.contains(token) ||
      client->second->pending_fds.erase(token) == 0)
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  return BLUEZ_MEDIA_SUCCESS;
} catch (...) {
  return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
}

void bluez_media_release_fd_token(void* token) try {
  auto& state = registry();
  const std::scoped_lock lock(state.mutex);
  const auto entry = state.fds.find(reinterpret_cast<uintptr_t>(token));
  if (entry == state.fds.end())
    return;
  ::close(entry->second);
  for (const auto& [handle, client] : state.clients)
    client->pending_fds.erase(reinterpret_cast<uintptr_t>(token));
  state.fds.erase(entry);
} catch (...) {
  // Native finalizers must never unwind across the C ABI.
}

int bluez_media_close_fd(int32_t fd) try {
  if (fd < 0) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }
  auto& state = registry();
  const std::scoped_lock lock(state.mutex);
  for (auto it = state.fds.begin(); it != state.fds.end(); ++it) {
    if (it->second == fd) {
      for (const auto& [handle, client] : state.clients)
        client->pending_fds.erase(it->first);
      state.fds.erase(it);
      break;
    }
  }
  return close(fd) == 0 ? BLUEZ_MEDIA_SUCCESS
                        : BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
} catch (...) {
  return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
}

}  // extern "C"
