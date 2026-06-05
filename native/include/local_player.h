// local_player.h
#pragma once

#include <map>
#include <sdbus-c++/sdbus-c++.h>
#include <string>

#include "bluez_media_native.h"

struct MediaPlayerState {
  std::string adapter_path;
  std::string player_path;
  std::string name;
  std::string type;
  std::string subtype;
  bool browsable{};
  bool searchable{};
};

class LocalPlayer {
public:
  LocalPlayer(sdbus::IConnection &conn,
              const BluezMediaPlayerRegistration &registration);
  ~LocalPlayer();

private:
  static constexpr auto kBluezService = "org.bluez";
  static constexpr auto kMediaIface = "org.bluez.Media1";

  std::map<std::string, sdbus::Variant> make_player_properties() const;

  sdbus::IConnection &conn_;
  MediaPlayerState state_;
};
