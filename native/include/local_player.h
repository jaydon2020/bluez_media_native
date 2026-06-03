// local_player.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <map>
#include <memory>
#include <string>

#include "bluez_media_native.h"

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

class LocalPlayer {
public:
  LocalPlayer(sdbus::IConnection &conn, const BluezMediaPlayerRegistration &registration);
  ~LocalPlayer();

private:
  static constexpr auto kBluezService = "org.bluez";
  static constexpr auto kMediaIface = "org.bluez.Media1";

  std::map<std::string, sdbus::Variant> make_player_properties() const;

  sdbus::IConnection &conn_;
  MediaPlayerState state_;
  std::unique_ptr<sdbus::IObject> object_;
};
