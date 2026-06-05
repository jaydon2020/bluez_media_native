// media_player_proxy.h
#pragma once

#include <cstdint>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <vector>

class MediaPlayerProxy {
public:
  MediaPlayerProxy(sdbus::IConnection &conn, const std::string &player_path);

  int play() const;
  int pause() const;
  int stop() const;
  int next() const;
  int previous() const;
  int set_repeat(const std::string &repeat) const;
  int set_shuffle(const std::string &shuffle) const;
  std::vector<uint8_t> properties() const;

private:
  static constexpr auto kBluezService = "org.bluez";
  static constexpr auto kMediaPlayerIface = "org.bluez.MediaPlayer1";

  std::string player_path_;
  std::unique_ptr<sdbus::IProxy> proxy_;
};
