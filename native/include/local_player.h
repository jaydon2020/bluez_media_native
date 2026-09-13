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
  std::string name;
  std::string type;
  std::string subtype;
};

class LocalPlayer {
 public:
  LocalPlayer(sdbus::IConnection& conn,
              const BluezMediaPlayerRegistration& registration);
  ~LocalPlayer();
  void abandon() { registered_ = false; }

  const std::string& adapter_path() const { return state_.adapter_path; }

 private:
  static constexpr auto kBluezService = "org.bluez";
  static constexpr auto kMediaIface = "org.bluez.Media1";
  static constexpr auto kMprisPlayerIface = "org.mpris.MediaPlayer2.Player";

  std::map<std::string, sdbus::Variant> make_player_properties() const;
  void register_mpris_object();

  sdbus::IConnection& conn_;
  MediaPlayerState state_;
  bool registered_ = true;
  std::string playback_status_{"Stopped"};
  std::string loop_status_{"None"};
  bool shuffle_{};
  int64_t position_{};
  std::unique_ptr<sdbus::IObject> object_;
};
