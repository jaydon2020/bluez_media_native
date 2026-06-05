// bluez_media_client.cpp — C ABI entry points for BlueZ Media registration.

#include "bluez_media_native.h"

#include <sdbus-c++/sdbus-c++.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <unistd.h>

#include "media_browser_proxy.h"
#include "media_client.h"
#include "media_control_proxy.h"
#include "media_object_manager.h"
#include "media_player_proxy.h"
#include "media_transport_proxy.h"

namespace {

bool is_invalid_media_player_parameter(const sdbus::Error &error) {
  return error.getName() == "org.bluez.Error.Failed" &&
         error.getMessage().find("Invalid Parameter") != std::string::npos;
}

} // namespace

struct BluezMediaClientContext {
  std::unique_ptr<sdbus::IConnection> conn;
  std::unique_ptr<MediaClient> client;
  std::unique_ptr<MediaObjectManager> object_manager;
};

extern "C" {

void bluez_media_init(void *dart_api_dl_data) {
  Dart_InitializeApiDL(dart_api_dl_data);
}

void *bluez_media_client_create(int64_t events_port) {
  try {
    auto ctx = std::make_unique<BluezMediaClientContext>();
    ctx->conn = sdbus::createSystemBusConnection();
    ctx->client = std::make_unique<MediaClient>(*ctx->conn);
    ctx->object_manager =
        std::make_unique<MediaObjectManager>(*ctx->conn, events_port);
    ctx->object_manager->get_managed_objects();
    ctx->conn->enterEventLoopAsync();
    return ctx.release();
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_client_create: %s\n", e.what());
    return nullptr;
  }
}

void bluez_media_client_destroy(void *handle) {
  auto *ctx = static_cast<BluezMediaClientContext *>(handle);
  if (ctx == nullptr) {
    return;
  }
  ctx->conn->leaveEventLoop();
  delete ctx; // NOLINT(cppcoreguidelines-owning-memory)
}

int bluez_media_register_player(
    void *handle, const BluezMediaPlayerRegistration *registration) {
  if (handle == nullptr || registration == nullptr) {
    return -1;
  }

  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    return ctx->client->register_player(*registration);
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_register_player: %s\n", e.what());
    return -3;
  }
}

int bluez_media_unregister_player(void *handle, const char *adapter_path,
                                  const char *player_path) {
  if (handle == nullptr) {
    return -1;
  }

  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    return ctx->client->unregister_player(adapter_path, player_path);
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_unregister_player: %s\n", e.what());
    return -3;
  }
}

int bluez_media_player_play(void *handle, const char *player_path) {
  if (handle == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return proxy.play();
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_player_play: %s\n", e.what());
    return -3;
  }
}

int bluez_media_player_pause(void *handle, const char *player_path) {
  if (handle == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return proxy.pause();
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_player_pause: %s\n", e.what());
    return -3;
  }
}

int bluez_media_player_stop(void *handle, const char *player_path) {
  if (handle == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return proxy.stop();
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_player_stop: %s\n", e.what());
    return -3;
  }
}

int bluez_media_player_next(void *handle, const char *player_path) {
  if (handle == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return proxy.next();
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_player_next: %s\n", e.what());
    return -3;
  }
}

int bluez_media_player_previous(void *handle, const char *player_path) {
  if (handle == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return proxy.previous();
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_player_previous: %s\n", e.what());
    return -3;
  }
}

int bluez_media_player_set_repeat(void *handle, const char *player_path,
                                  const char *repeat) {
  if (handle == nullptr || player_path == nullptr || repeat == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return proxy.set_repeat(repeat);
  } catch (const sdbus::Error &e) {
    if (is_invalid_media_player_parameter(e)) {
      return -4;
    }
    fprintf(stderr, "bluez_media_player_set_repeat: %s\n", e.what());
    return -3;
  }
}

int bluez_media_player_set_shuffle(void *handle, const char *player_path,
                                   const char *shuffle) {
  if (handle == nullptr || player_path == nullptr || shuffle == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    return proxy.set_shuffle(shuffle);
  } catch (const sdbus::Error &e) {
    if (is_invalid_media_player_parameter(e)) {
      return -4;
    }
    fprintf(stderr, "bluez_media_player_set_shuffle: %s\n", e.what());
    return -3;
  }
}

int bluez_media_player_get_properties(void *handle, const char *player_path,
                                      uint8_t *out, int32_t capacity) {
  if (handle == nullptr || capacity < 0) {
    return -1;
  }

  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaPlayerProxy proxy{*ctx->conn, player_path};
    const auto payload = proxy.properties();
    if (payload.empty()) {
      return -1;
    }
    if (out == nullptr || capacity == 0) {
      return static_cast<int>(payload.size());
    }
    if (capacity < static_cast<int32_t>(payload.size())) {
      return -2;
    }
    std::memcpy(out, payload.data(), payload.size());
    return static_cast<int>(payload.size());
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_player_get_properties: %s\n", e.what());
    return -3;
  }
}

int bluez_media_control_play(void *handle, const char *control_path) {
  if (handle == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.play();
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_control_play: %s\n", e.what());
    return -3;
  }
}

int bluez_media_control_pause(void *handle, const char *control_path) {
  if (handle == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.pause();
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_control_pause: %s\n", e.what());
    return -3;
  }
}

int bluez_media_control_stop(void *handle, const char *control_path) {
  if (handle == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.stop();
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_control_stop: %s\n", e.what());
    return -3;
  }
}

int bluez_media_control_next(void *handle, const char *control_path) {
  if (handle == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.next();
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_control_next: %s\n", e.what());
    return -3;
  }
}

int bluez_media_control_previous(void *handle, const char *control_path) {
  if (handle == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.previous();
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_control_previous: %s\n", e.what());
    return -3;
  }
}

int bluez_media_control_volume_up(void *handle, const char *control_path) {
  if (handle == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.volume_up();
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_control_volume_up: %s\n", e.what());
    return -3;
  }
}

int bluez_media_control_volume_down(void *handle, const char *control_path) {
  if (handle == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.volume_down();
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_control_volume_down: %s\n", e.what());
    return -3;
  }
}

int bluez_media_control_fast_forward(void *handle, const char *control_path) {
  if (handle == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.fast_forward();
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_control_fast_forward: %s\n", e.what());
    return -3;
  }
}

int bluez_media_control_rewind(void *handle, const char *control_path) {
  if (handle == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaControlProxy proxy{*ctx->conn, control_path};
    return proxy.rewind();
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_control_rewind: %s\n", e.what());
    return -3;
  }
}

int bluez_media_control_get_properties(void *handle, const char *control_path,
                                       uint8_t *out, int32_t capacity) {
  if (handle == nullptr || capacity < 0) {
    return -1;
  }

  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaControlProxy proxy{*ctx->conn, control_path};
    const auto payload = proxy.properties();
    if (payload.empty()) {
      return -1;
    }
    if (out == nullptr || capacity == 0) {
      return static_cast<int>(payload.size());
    }
    if (capacity < static_cast<int32_t>(payload.size())) {
      return -2;
    }
    std::memcpy(out, payload.data(), payload.size());
    return static_cast<int>(payload.size());
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_control_get_properties: %s\n", e.what());
    return -3;
  }
}

int bluez_media_folder_search(void *handle, const char *folder_path,
                              const char *value, uint8_t *out,
                              int32_t capacity) {
  if (handle == nullptr || capacity < 0) {
    return -1;
  }

  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaBrowserProxy proxy{*ctx->conn};
    const auto payload =
        proxy.folder_search(folder_path, value != nullptr ? value : "");
    if (payload.empty()) {
      return -1;
    }
    if (out == nullptr || capacity == 0) {
      return static_cast<int>(payload.size());
    }
    if (capacity < static_cast<int32_t>(payload.size())) {
      return -2;
    }
    std::memcpy(out, payload.data(), payload.size());
    return static_cast<int>(payload.size());
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_folder_search: %s\n", e.what());
    return -3;
  }
}

int bluez_media_folder_list_items(void *handle, const char *folder_path,
                                  uint8_t *out, int32_t capacity) {
  if (handle == nullptr || capacity < 0) {
    return -1;
  }

  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaBrowserProxy proxy{*ctx->conn};
    const auto payload = proxy.folder_list_items(folder_path);
    if (payload.empty()) {
      return -1;
    }
    if (out == nullptr || capacity == 0) {
      return static_cast<int>(payload.size());
    }
    if (capacity < static_cast<int32_t>(payload.size())) {
      return -2;
    }
    std::memcpy(out, payload.data(), payload.size());
    return static_cast<int>(payload.size());
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_folder_list_items: %s\n", e.what());
    return -3;
  }
}

int bluez_media_folder_change_folder(void *handle, const char *folder_path,
                                     const char *target_folder_path) {
  if (handle == nullptr || folder_path == nullptr ||
      target_folder_path == nullptr) {
    return -1;
  }

  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaBrowserProxy proxy{*ctx->conn};
    return proxy.folder_change_folder(folder_path, target_folder_path);
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_folder_change_folder: %s\n", e.what());
    return -3;
  }
}

int bluez_media_folder_get_properties(void *handle, const char *folder_path,
                                      uint8_t *out, int32_t capacity) {
  if (handle == nullptr || capacity < 0) {
    return -1;
  }

  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaBrowserProxy proxy{*ctx->conn};
    const auto payload = proxy.folder_properties(folder_path);
    if (payload.empty()) {
      return -1;
    }
    if (out == nullptr || capacity == 0) {
      return static_cast<int>(payload.size());
    }
    if (capacity < static_cast<int32_t>(payload.size())) {
      return -2;
    }
    std::memcpy(out, payload.data(), payload.size());
    return static_cast<int>(payload.size());
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_folder_get_properties: %s\n", e.what());
    return -3;
  }
}

int bluez_media_item_play(void *handle, const char *item_path) {
  if (handle == nullptr || item_path == nullptr) {
    return -1;
  }

  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaBrowserProxy proxy{*ctx->conn};
    return proxy.item_play(item_path);
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_item_play: %s\n", e.what());
    return -3;
  }
}

int bluez_media_item_add_to_now_playing(void *handle, const char *item_path) {
  if (handle == nullptr || item_path == nullptr) {
    return -1;
  }

  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaBrowserProxy proxy{*ctx->conn};
    return proxy.item_add_to_now_playing(item_path);
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_item_add_to_now_playing: %s\n", e.what());
    return -3;
  }
}

int bluez_media_item_get_properties(void *handle, const char *item_path,
                                    uint8_t *out, int32_t capacity) {
  if (handle == nullptr || capacity < 0) {
    return -1;
  }

  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaBrowserProxy proxy{*ctx->conn};
    const auto payload = proxy.item_properties(item_path);
    if (payload.empty()) {
      return -1;
    }
    if (out == nullptr || capacity == 0) {
      return static_cast<int>(payload.size());
    }
    if (capacity < static_cast<int32_t>(payload.size())) {
      return -2;
    }
    std::memcpy(out, payload.data(), payload.size());
    return static_cast<int>(payload.size());
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_item_get_properties: %s\n", e.what());
    return -3;
  }
}

// ── org.bluez.MediaTransport1 remote transports ────────────────────────────

int bluez_media_transport_acquire(void *handle, const char *transport_path,
                                  uint8_t *out, int32_t capacity) {
  if (handle == nullptr || capacity < 0 || transport_path == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaTransportProxy proxy{*ctx->conn, transport_path};
    const auto payload = proxy.acquire();
    if (payload.empty()) {
      return -1;
    }
    if (out == nullptr || capacity == 0) {
      return static_cast<int>(payload.size());
    }
    if (capacity < static_cast<int32_t>(payload.size())) {
      return -2;
    }
    std::memcpy(out, payload.data(), payload.size());
    return static_cast<int>(payload.size());
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_transport_acquire: %s\n", e.what());
    return -3;
  }
}

int bluez_media_transport_try_acquire(void *handle, const char *transport_path,
                                      uint8_t *out, int32_t capacity) {
  if (handle == nullptr || capacity < 0 || transport_path == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaTransportProxy proxy{*ctx->conn, transport_path};
    const auto payload = proxy.try_acquire();
    if (payload.empty()) {
      return -1;
    }
    if (out == nullptr || capacity == 0) {
      return static_cast<int>(payload.size());
    }
    if (capacity < static_cast<int32_t>(payload.size())) {
      return -2;
    }
    std::memcpy(out, payload.data(), payload.size());
    return static_cast<int>(payload.size());
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_transport_try_acquire: %s\n", e.what());
    return -3;
  }
}

int bluez_media_transport_release(void *handle, const char *transport_path) {
  if (handle == nullptr || transport_path == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaTransportProxy proxy{*ctx->conn, transport_path};
    return proxy.release();
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_transport_release: %s\n", e.what());
    return -3;
  }
}

int bluez_media_transport_get_properties(void *handle,
                                         const char *transport_path,
                                         uint8_t *out, int32_t capacity) {
  if (handle == nullptr || capacity < 0 || transport_path == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaTransportProxy proxy{*ctx->conn, transport_path};
    const auto payload = proxy.properties();
    if (payload.empty()) {
      return -1;
    }
    if (out == nullptr || capacity == 0) {
      return static_cast<int>(payload.size());
    }
    if (capacity < static_cast<int32_t>(payload.size())) {
      return -2;
    }
    std::memcpy(out, payload.data(), payload.size());
    return static_cast<int>(payload.size());
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_transport_get_properties: %s\n", e.what());
    return -3;
  }
}

int bluez_media_transport_set_volume(void *handle, const char *transport_path,
                                     uint16_t volume) {
  if (handle == nullptr || transport_path == nullptr) {
    return -1;
  }
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    MediaTransportProxy proxy{*ctx->conn, transport_path};
    return proxy.set_volume(volume);
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_transport_set_volume: %s\n", e.what());
    return -3;
  }
}

int bluez_media_get_managed_objects(void *handle, uint8_t *out,
                                    int32_t capacity) {
  if (handle == nullptr || capacity < 0)
    return -1;
  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    std::vector<uint8_t> result = ctx->client->get_managed_objects();
    if (result.empty()) {
      return -1;
    }
    if (out == nullptr || capacity == 0) {
      return static_cast<int>(result.size());
    }
    if (capacity < static_cast<int32_t>(result.size())) {
      return -2;
    }
    std::memcpy(out, result.data(), result.size());
    return static_cast<int>(result.size());
  } catch (const std::exception &e) {
    fprintf(stderr, "bluez_media_get_managed_objects: %s\n", e.what());
    return -3;
  }
}

int bluez_media_close_fd(int32_t fd) {
  if (fd < 0) {
    return -1;
  }
  return close(fd) == 0 ? 0 : -3;
}

} // extern "C"
