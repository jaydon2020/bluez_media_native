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
  const char *identity;
  const char *name;
  const char *type;
  const char *subtype;
  const char *status;
  uint32_t position_ms;
  uint8_t can_go_next;
  uint8_t can_go_previous;
  uint8_t can_play;
  uint8_t can_pause;
  uint8_t can_seek;
  uint8_t can_control;
  uint8_t browsable;
  uint8_t searchable;
} BluezMediaPlayerRegistration;

// ── Client lifecycle ────────────────────────────────────────────────────────

BLUEZ_MEDIA_EXPORT void *bluez_media_client_create(void);
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

// A very short-lived native function.
//
// For very short-lived functions, it is fine to call them on the main isolate.
// They block Dart execution while running, so only use this pattern for work
// that is guaranteed to complete quickly.
BLUEZ_MEDIA_EXPORT int sum(int a, int b);

// A longer-lived native function that occupies the calling thread.
//
// Do not call this kind of function on the main isolate in Flutter apps. Use a
// helper isolate for blocking native work.
BLUEZ_MEDIA_EXPORT int sum_long_running(int a, int b);

#ifdef __cplusplus
}
#endif
