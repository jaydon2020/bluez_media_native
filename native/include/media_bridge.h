// media_bridge.h — org.bluez.Media1 registration bridge.
//
// Owns local player object paths and calls Media1.RegisterPlayer /
// Media1.UnregisterPlayer on a BlueZ adapter.

#pragma once

#include "bluez_media_native.h"

#include <sdbus-c++/sdbus-c++.h>

#include <map>
#include <memory>
#include <string>

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

  [[nodiscard]] bool has_player(const std::string &player_path) const;
  [[nodiscard]] size_t registered_player_count() const;

private:
  static constexpr auto kBluezService = "org.bluez";
  static constexpr auto kMediaIface = "org.bluez.Media1";
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

  sdbus::IConnection &conn_;
  std::map<std::string, MediaPlayerRegistrationRecord> players_;
};
