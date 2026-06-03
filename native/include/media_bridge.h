// media_bridge.h — org.bluez.Media1 registration bridge.
//
// Owns local player object paths and calls Media1.RegisterPlayer /
// Media1.UnregisterPlayer on a BlueZ adapter.

#pragma once

#include "bluez_media_native.h"
#include "bluez_media_types.h"

#include <sdbus-c++/sdbus-c++.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

struct MediaPlayerState {
  std::string adapter_path;
  std::string player_path;
  std::string identity;
  std::string name;
  std::string type;
  std::string subtype;
  std::string status;
  uint32_t position_ms{};
  bool can_go_next{};
  bool can_go_previous{};
  bool can_play{};
  bool can_pause{};
  bool can_seek{};
  bool can_control{};
  bool browsable{};
  bool searchable{};
};

struct MediaPlayerRegistrationRecord {
  MediaPlayerState state;
  std::unique_ptr<sdbus::IObject> object;
};

class MediaBridge {
public:
  explicit MediaBridge(sdbus::IConnection &conn);

  int register_player(const BluezMediaPlayerRegistration &registration);
  int unregister_player(const char *adapter_path, const char *player_path);

  int player_play(const char *player_path);
  int player_pause(const char *player_path);
  int player_stop(const char *player_path);
  int player_next(const char *player_path);
  int player_previous(const char *player_path);
  std::vector<uint8_t> player_properties(const char *player_path);

  int control_play(const char *control_path);
  int control_pause(const char *control_path);
  int control_stop(const char *control_path);
  int control_next(const char *control_path);
  int control_previous(const char *control_path);
  int control_volume_up(const char *control_path);
  int control_volume_down(const char *control_path);
  int control_fast_forward(const char *control_path);
  int control_rewind(const char *control_path);
  std::vector<uint8_t> control_properties(const char *control_path);

  std::vector<uint8_t> folder_search(const char *folder_path,
                                     const char *value);
  std::vector<uint8_t> folder_list_items(const char *folder_path);
  int folder_change_folder(const char *folder_path,
                           const char *target_folder_path);
  std::vector<uint8_t> folder_properties(const char *folder_path);

  int item_play(const char *item_path);
  int item_add_to_now_playing(const char *item_path);
  std::vector<uint8_t> item_properties(const char *item_path);

  std::vector<uint8_t> transport_acquire(const char *transport_path);
  std::vector<uint8_t> transport_try_acquire(const char *transport_path);
  int transport_release(const char *transport_path);
  std::vector<uint8_t> transport_properties(const char *transport_path);
  int transport_set_volume(const char *transport_path, uint16_t volume);

  [[nodiscard]] bool has_player(const std::string &player_path) const;
  [[nodiscard]] size_t registered_player_count() const;

private:
  static constexpr auto kBluezService = "org.bluez";
  static constexpr auto kMediaIface = "org.bluez.Media1";
  static constexpr auto kMediaControlIface = "org.bluez.MediaControl1";
  static constexpr auto kMediaFolderIface = "org.bluez.MediaFolder1";
  static constexpr auto kMediaItemIface = "org.bluez.MediaItem1";
  static constexpr auto kMediaPlayerIface = "org.bluez.MediaPlayer1";
  static constexpr auto kMediaTransportIface = "org.bluez.MediaTransport1";
  static constexpr auto kMprisPlayerIface = "org.mpris.MediaPlayer2.Player";
  static constexpr auto kMprisRootIface = "org.mpris.MediaPlayer2";

  static MediaPlayerState
  make_state(const BluezMediaPlayerRegistration &registration);
  static void add_mpris_root_vtable(sdbus::IObject &object,
                                    const MediaPlayerState &state);
  static void add_mpris_player_vtable(sdbus::IObject &object,
                                      const MediaPlayerState &state);
  static std::map<std::string, sdbus::Variant>
  make_player_properties(const MediaPlayerState &state);
  static std::string variant_to_string(const sdbus::Variant &value);
  static std::vector<BlueZMediaProperty>
  track_to_properties(const std::map<std::string, sdbus::Variant> &track);
  static BlueZMediaItemProps
  item_from_properties(const std::string &object_path,
                       const std::map<std::string, sdbus::Variant> &props);

  [[nodiscard]] std::unique_ptr<sdbus::IProxy>
  make_player_proxy(const char *player_path) const;
  [[nodiscard]] std::unique_ptr<sdbus::IProxy>
  make_control_proxy(const char *control_path) const;
  [[nodiscard]] std::unique_ptr<sdbus::IProxy>
  make_folder_proxy(const char *folder_path) const;
  [[nodiscard]] std::unique_ptr<sdbus::IProxy>
  make_item_proxy(const char *item_path) const;
  [[nodiscard]] std::unique_ptr<sdbus::IProxy>
  make_transport_proxy(const char *transport_path) const;

  sdbus::IConnection &conn_;
  std::map<std::string, MediaPlayerRegistrationRecord> players_;
};
