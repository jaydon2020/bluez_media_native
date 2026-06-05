#pragma once

#include <stdint.h>

#if _WIN32
#define BLUEZ_MEDIA_EXPORT __declspec(dllexport)
#else
#define BLUEZ_MEDIA_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BluezMediaPlayerRegistration {
  const char *adapter_path;
  const char *player_path;
  const char *name;
  const char *type;
  const char *subtype;
  uint8_t browsable;
  uint8_t searchable;
} BluezMediaPlayerRegistration;

// ── Client lifecycle ────────────────────────────────────────────────────────

BLUEZ_MEDIA_EXPORT void bluez_media_init(void *dart_api_dl_data);
BLUEZ_MEDIA_EXPORT void *bluez_media_client_create(int64_t events_port);
BLUEZ_MEDIA_EXPORT void bluez_media_client_destroy(void *handle);

// ── org.bluez.Media1 registration ──────────────────────────────────────────

BLUEZ_MEDIA_EXPORT int
bluez_media_register_player(void *handle,
                            const BluezMediaPlayerRegistration *registration);
BLUEZ_MEDIA_EXPORT int bluez_media_unregister_player(void *handle,
                                                     const char *adapter_path,
                                                     const char *player_path);

// ── org.bluez.MediaPlayer1 remote controls ─────────────────────────────────

BLUEZ_MEDIA_EXPORT int bluez_media_player_play(void *handle,
                                               const char *player_path);
BLUEZ_MEDIA_EXPORT int bluez_media_player_pause(void *handle,
                                                const char *player_path);
BLUEZ_MEDIA_EXPORT int bluez_media_player_stop(void *handle,
                                               const char *player_path);
BLUEZ_MEDIA_EXPORT int bluez_media_player_next(void *handle,
                                               const char *player_path);
BLUEZ_MEDIA_EXPORT int bluez_media_player_previous(void *handle,
                                                   const char *player_path);
BLUEZ_MEDIA_EXPORT int
bluez_media_player_get_properties(void *handle, const char *player_path,
                                  uint8_t *out, int32_t capacity);

// ── org.bluez.MediaControl1 remote controller controls ─────────────────────

BLUEZ_MEDIA_EXPORT int bluez_media_control_play(void *handle,
                                                const char *control_path);
BLUEZ_MEDIA_EXPORT int bluez_media_control_pause(void *handle,
                                                 const char *control_path);
BLUEZ_MEDIA_EXPORT int bluez_media_control_stop(void *handle,
                                                const char *control_path);
BLUEZ_MEDIA_EXPORT int bluez_media_control_next(void *handle,
                                                const char *control_path);
BLUEZ_MEDIA_EXPORT int bluez_media_control_previous(void *handle,
                                                    const char *control_path);
BLUEZ_MEDIA_EXPORT int bluez_media_control_volume_up(void *handle,
                                                     const char *control_path);
BLUEZ_MEDIA_EXPORT int
bluez_media_control_volume_down(void *handle, const char *control_path);
BLUEZ_MEDIA_EXPORT int
bluez_media_control_fast_forward(void *handle, const char *control_path);
BLUEZ_MEDIA_EXPORT int bluez_media_control_rewind(void *handle,
                                                  const char *control_path);
BLUEZ_MEDIA_EXPORT int
bluez_media_control_get_properties(void *handle, const char *control_path,
                                   uint8_t *out, int32_t capacity);

// ── org.bluez.MediaFolder1 browsing ────────────────────────────────────────

BLUEZ_MEDIA_EXPORT int
bluez_media_folder_search(void *handle, const char *folder_path,
                          const char *value, uint8_t *out, int32_t capacity);
BLUEZ_MEDIA_EXPORT int bluez_media_folder_list_items(void *handle,
                                                     const char *folder_path,
                                                     uint8_t *out,
                                                     int32_t capacity);
BLUEZ_MEDIA_EXPORT int
bluez_media_folder_change_folder(void *handle, const char *folder_path,
                                 const char *target_folder_path);
BLUEZ_MEDIA_EXPORT int
bluez_media_folder_get_properties(void *handle, const char *folder_path,
                                  uint8_t *out, int32_t capacity);

// ── org.bluez.MediaItem1 browsing ──────────────────────────────────────────

BLUEZ_MEDIA_EXPORT int bluez_media_item_play(void *handle,
                                             const char *item_path);
BLUEZ_MEDIA_EXPORT int
bluez_media_item_add_to_now_playing(void *handle, const char *item_path);
BLUEZ_MEDIA_EXPORT int bluez_media_item_get_properties(void *handle,
                                                       const char *item_path,
                                                       uint8_t *out,
                                                       int32_t capacity);

// ── org.bluez.MediaTransport1 remote transports ────────────────────────────

BLUEZ_MEDIA_EXPORT int bluez_media_transport_acquire(void *handle,
                                                     const char *transport_path,
                                                     uint8_t *out,
                                                     int32_t capacity);
BLUEZ_MEDIA_EXPORT int
bluez_media_transport_try_acquire(void *handle, const char *transport_path,
                                  uint8_t *out, int32_t capacity);
BLUEZ_MEDIA_EXPORT int
bluez_media_transport_release(void *handle, const char *transport_path);
BLUEZ_MEDIA_EXPORT int
bluez_media_transport_get_properties(void *handle, const char *transport_path,
                                     uint8_t *out, int32_t capacity);
BLUEZ_MEDIA_EXPORT int
bluez_media_transport_set_volume(void *handle, const char *transport_path,
                                 uint16_t volume);
BLUEZ_MEDIA_EXPORT int bluez_media_close_fd(int32_t fd);

// ── ObjectManager queries ──────────────────────────────────────────────────

BLUEZ_MEDIA_EXPORT int
bluez_media_get_managed_objects(void *handle, uint8_t *out, int32_t capacity);

#ifdef __cplusplus
}
#endif
