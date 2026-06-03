// bluez_media_client.cpp — C ABI entry points for BlueZ Media registration.

#include "bluez_media_native.h"

#include <sdbus-c++/sdbus-c++.h>

#include <cstdio>
#include <cstring>
#include <memory>

#include "media_bridge.h"

struct BluezMediaClientContext {
  std::unique_ptr<sdbus::IConnection> conn;
  std::unique_ptr<MediaBridge> media;
};

extern "C" {

void *bluez_media_client_create(void) {
  try {
    auto ctx = std::make_unique<BluezMediaClientContext>();
    ctx->conn = sdbus::createSystemBusConnection();
    ctx->media = std::make_unique<MediaBridge>(*ctx->conn);
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
    return ctx->media->register_player(*registration);
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
    return ctx->media->unregister_player(adapter_path, player_path);
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
    return ctx->media->player_play(player_path);
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
    return ctx->media->player_pause(player_path);
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
    return ctx->media->player_stop(player_path);
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
    return ctx->media->player_next(player_path);
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
    return ctx->media->player_previous(player_path);
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "bluez_media_player_previous: %s\n", e.what());
    return -3;
  }
}

int bluez_media_player_get_properties(void *handle, const char *player_path,
                                      uint8_t *out, int32_t capacity) {
  if (handle == nullptr) {
    return -1;
  }
  if (capacity < 0) {
    return -1;
  }

  try {
    auto *ctx = static_cast<BluezMediaClientContext *>(handle);
    const auto payload = ctx->media->player_properties(player_path);
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

} // extern "C"
