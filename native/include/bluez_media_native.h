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
  const char* adapter_path;
  const char* player_path;
  const char* name;
  const char* type;
  const char* subtype;
  uint8_t browsable;
  uint8_t searchable;
} BluezMediaPlayerRegistration;

typedef enum BluezMediaStatusCode {
  BLUEZ_MEDIA_SUCCESS = 0,
  BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT = -1,
  BLUEZ_MEDIA_ERROR_BUFFER_TOO_SMALL = -2,
  BLUEZ_MEDIA_ERROR_OPERATION_FAILED = -3,
  BLUEZ_MEDIA_ERROR_UNSUPPORTED_SETTING = -4,
  BLUEZ_MEDIA_ERROR_ALREADY_EXISTS = -5,
  BLUEZ_MEDIA_ERROR_NOT_FOUND = -6,
} BluezMediaStatusCode;

typedef int32_t BluezMediaOperation;
#define BLUEZ_MEDIA_OP_PLAYER_PLAY 0
#define BLUEZ_MEDIA_OP_PLAYER_PAUSE 1
#define BLUEZ_MEDIA_OP_PLAYER_STOP 2
#define BLUEZ_MEDIA_OP_PLAYER_NEXT 3
#define BLUEZ_MEDIA_OP_PLAYER_PREVIOUS 4
#define BLUEZ_MEDIA_OP_PLAYER_FAST_FORWARD 5
#define BLUEZ_MEDIA_OP_PLAYER_REWIND 6
#define BLUEZ_MEDIA_OP_PLAYER_SET_REPEAT 7
#define BLUEZ_MEDIA_OP_PLAYER_SET_SHUFFLE 8
#define BLUEZ_MEDIA_OP_PLAYER_GET_PROPERTIES 9
#define BLUEZ_MEDIA_OP_PLAYER_GET_COVER_ART 10
#define BLUEZ_MEDIA_OP_CONTROL_PLAY 11
#define BLUEZ_MEDIA_OP_CONTROL_PAUSE 12
#define BLUEZ_MEDIA_OP_CONTROL_STOP 13
#define BLUEZ_MEDIA_OP_CONTROL_NEXT 14
#define BLUEZ_MEDIA_OP_CONTROL_PREVIOUS 15
#define BLUEZ_MEDIA_OP_CONTROL_VOLUME_UP 16
#define BLUEZ_MEDIA_OP_CONTROL_VOLUME_DOWN 17
#define BLUEZ_MEDIA_OP_CONTROL_FAST_FORWARD 18
#define BLUEZ_MEDIA_OP_CONTROL_REWIND 19
#define BLUEZ_MEDIA_OP_CONTROL_GET_PROPERTIES 20
#define BLUEZ_MEDIA_OP_FOLDER_SEARCH 21
#define BLUEZ_MEDIA_OP_FOLDER_LIST_ITEMS 22
#define BLUEZ_MEDIA_OP_FOLDER_CHANGE_FOLDER 23
#define BLUEZ_MEDIA_OP_FOLDER_GET_PROPERTIES 24
#define BLUEZ_MEDIA_OP_ITEM_PLAY 25
#define BLUEZ_MEDIA_OP_ITEM_ADD_TO_NOW_PLAYING 26
#define BLUEZ_MEDIA_OP_ITEM_GET_PROPERTIES 27
#define BLUEZ_MEDIA_OP_TRANSPORT_ACQUIRE 28
#define BLUEZ_MEDIA_OP_TRANSPORT_TRY_ACQUIRE 29
#define BLUEZ_MEDIA_OP_TRANSPORT_RELEASE 30
#define BLUEZ_MEDIA_OP_TRANSPORT_GET_PROPERTIES 31
#define BLUEZ_MEDIA_OP_TRANSPORT_SET_VOLUME 32
#define BLUEZ_MEDIA_OP_GET_MANAGED_OBJECTS 33
#define BLUEZ_MEDIA_OP_UNREGISTER_PLAYER 34
#define BLUEZ_MEDIA_OP_PLAYER_GET_COVER_ART_FROM_EXISTING_SESSION 35
#define BLUEZ_MEDIA_OP_ITEM_GET_COVER_ART 36
#define BLUEZ_MEDIA_OP_ITEM_GET_COVER_ART_FROM_EXISTING_SESSION 37

typedef struct BluezMediaBuffer {
  uint8_t* data;
  int32_t length;
} BluezMediaBuffer;

// ── Client lifecycle ────────────────────────────────────────────────────────

// Absolute loader path of this native asset; borrowed for the library lifetime.
BLUEZ_MEDIA_EXPORT const char* bluez_media_library_path(void);
BLUEZ_MEDIA_EXPORT void bluez_media_init(void* dart_api_dl_data);
BLUEZ_MEDIA_EXPORT void* bluez_media_client_create(int64_t events_port);
BLUEZ_MEDIA_EXPORT void bluez_media_client_create_async(int64_t events_port,
                                                        int64_t result_port);
BLUEZ_MEDIA_EXPORT void bluez_media_client_destroy(void* handle);
// Retire immediately; report completion after queued calls and cleanup finish.
BLUEZ_MEDIA_EXPORT void bluez_media_client_destroy_async(void* handle,
                                                        int64_t result_port);
// Releases data returned through any BluezMediaBuffer and resets its fields.
BLUEZ_MEDIA_EXPORT void bluez_media_buffer_free(BluezMediaBuffer* buffer);
BLUEZ_MEDIA_EXPORT void bluez_media_call_async(void* handle,
                                               BluezMediaOperation operation,
                                               const char* object_path,
                                               const char* argument,
                                               int32_t value,
                                               int64_t result_port);

// ── org.bluez.Media1 registration ──────────────────────────────────────────

BLUEZ_MEDIA_EXPORT int bluez_media_register_player(
    void* handle,
    const BluezMediaPlayerRegistration* registration);
BLUEZ_MEDIA_EXPORT void bluez_media_register_player_async(
    void* handle,
    const BluezMediaPlayerRegistration* registration,
    int64_t result_port);
BLUEZ_MEDIA_EXPORT int bluez_media_unregister_player(void* handle,
                                                     const char* adapter_path,
                                                     const char* player_path);

// ── org.bluez.MediaPlayer1 remote controls ─────────────────────────────────

BLUEZ_MEDIA_EXPORT int bluez_media_player_play(void* handle,
                                               const char* player_path);
BLUEZ_MEDIA_EXPORT int bluez_media_player_pause(void* handle,
                                                const char* player_path);
BLUEZ_MEDIA_EXPORT int bluez_media_player_stop(void* handle,
                                               const char* player_path);
BLUEZ_MEDIA_EXPORT int bluez_media_player_next(void* handle,
                                               const char* player_path);
BLUEZ_MEDIA_EXPORT int bluez_media_player_previous(void* handle,
                                                   const char* player_path);
BLUEZ_MEDIA_EXPORT int bluez_media_player_fast_forward(void* handle,
                                                       const char* player_path);
BLUEZ_MEDIA_EXPORT int bluez_media_player_rewind(void* handle,
                                                 const char* player_path);
BLUEZ_MEDIA_EXPORT int bluez_media_player_set_repeat(void* handle,
                                                     const char* player_path,
                                                     const char* repeat);
BLUEZ_MEDIA_EXPORT int bluez_media_player_set_shuffle(void* handle,
                                                      const char* player_path,
                                                      const char* shuffle);
BLUEZ_MEDIA_EXPORT int bluez_media_player_get_properties(
    void* handle,
    const char* player_path,
    BluezMediaBuffer* out);
BLUEZ_MEDIA_EXPORT int bluez_media_player_get_cover_art(void* handle,
                                                        const char* player_path,
                                                        const char* target_file,
                                                        int32_t timeout_ms);
BLUEZ_MEDIA_EXPORT int bluez_media_player_get_cover_art_from_existing_session(
    void* handle,
    const char* player_path,
    const char* target_file,
    int32_t timeout_ms);

// ── org.bluez.MediaControl1 remote controller controls ─────────────────────

BLUEZ_MEDIA_EXPORT int bluez_media_control_play(void* handle,
                                                const char* control_path);
BLUEZ_MEDIA_EXPORT int bluez_media_control_pause(void* handle,
                                                 const char* control_path);
BLUEZ_MEDIA_EXPORT int bluez_media_control_stop(void* handle,
                                                const char* control_path);
BLUEZ_MEDIA_EXPORT int bluez_media_control_next(void* handle,
                                                const char* control_path);
BLUEZ_MEDIA_EXPORT int bluez_media_control_previous(void* handle,
                                                    const char* control_path);
BLUEZ_MEDIA_EXPORT int bluez_media_control_volume_up(void* handle,
                                                     const char* control_path);
BLUEZ_MEDIA_EXPORT int bluez_media_control_volume_down(
    void* handle,
    const char* control_path);
BLUEZ_MEDIA_EXPORT int bluez_media_control_fast_forward(
    void* handle,
    const char* control_path);
BLUEZ_MEDIA_EXPORT int bluez_media_control_rewind(void* handle,
                                                  const char* control_path);
BLUEZ_MEDIA_EXPORT int bluez_media_control_get_properties(
    void* handle,
    const char* control_path,
    BluezMediaBuffer* out);

// ── org.bluez.MediaFolder1 browsing ────────────────────────────────────────

BLUEZ_MEDIA_EXPORT int bluez_media_folder_search(void* handle,
                                                 const char* folder_path,
                                                 const char* value,
                                                 BluezMediaBuffer* out);
BLUEZ_MEDIA_EXPORT int bluez_media_folder_list_items(void* handle,
                                                     const char* folder_path,
                                                     BluezMediaBuffer* out);
BLUEZ_MEDIA_EXPORT int bluez_media_folder_change_folder(
    void* handle,
    const char* folder_path,
    const char* target_folder_path);
BLUEZ_MEDIA_EXPORT int bluez_media_folder_get_properties(
    void* handle,
    const char* folder_path,
    BluezMediaBuffer* out);

// ── org.bluez.MediaItem1 browsing ──────────────────────────────────────────

BLUEZ_MEDIA_EXPORT int bluez_media_item_play(void* handle,
                                             const char* item_path);
BLUEZ_MEDIA_EXPORT int bluez_media_item_add_to_now_playing(
    void* handle,
    const char* item_path);
BLUEZ_MEDIA_EXPORT int bluez_media_item_get_properties(void* handle,
                                                       const char* item_path,
                                                       BluezMediaBuffer* out);
BLUEZ_MEDIA_EXPORT int bluez_media_item_get_cover_art(void* handle,
                                                      const char* item_path,
                                                      const char* target_file,
                                                      int32_t timeout_ms);
BLUEZ_MEDIA_EXPORT int bluez_media_item_get_cover_art_from_existing_session(
    void* handle,
    const char* item_path,
    const char* target_file,
    int32_t timeout_ms);

// ── org.bluez.MediaTransport1 remote transports ────────────────────────────

BLUEZ_MEDIA_EXPORT int bluez_media_transport_acquire(void* handle,
                                                     const char* transport_path,
                                                     BluezMediaBuffer* out);
BLUEZ_MEDIA_EXPORT int bluez_media_transport_try_acquire(
    void* handle,
    const char* transport_path,
    BluezMediaBuffer* out);
BLUEZ_MEDIA_EXPORT int bluez_media_transport_release(
    void* handle,
    const char* transport_path);
BLUEZ_MEDIA_EXPORT int bluez_media_transport_get_properties(
    void* handle,
    const char* transport_path,
    BluezMediaBuffer* out);
BLUEZ_MEDIA_EXPORT int bluez_media_transport_set_volume(
    void* handle,
    const char* transport_path,
    uint16_t volume);
// Async Acquire returns tag 0x11, a uint64 little-endian ownership token,
// then the usual acquire payload. Install cleanup before claiming the token.
// Unclaimed tokens are closed with their client. Release is idempotent.
BLUEZ_MEDIA_EXPORT int bluez_media_claim_fd(void* handle, uint64_t token);
BLUEZ_MEDIA_EXPORT void bluez_media_release_fd_token(void* token);
BLUEZ_MEDIA_EXPORT int bluez_media_close_fd(int32_t fd);

// ── ObjectManager queries ──────────────────────────────────────────────────

BLUEZ_MEDIA_EXPORT int bluez_media_get_managed_objects(void* handle,
                                                       BluezMediaBuffer* out);

#ifdef __cplusplus
}
#endif
