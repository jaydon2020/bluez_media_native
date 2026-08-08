// media_player_proxy.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class MediaPlayerProxy {
 public:
  MediaPlayerProxy(sdbus::IConnection& conn, const std::string& player_path);

  int play() const;
  int pause() const;
  int stop() const;
  int next() const;
  int previous() const;
  int fast_forward() const;
  int rewind() const;
  int set_repeat(const std::string& repeat) const;
  int set_shuffle(const std::string& shuffle) const;
  std::vector<uint8_t> properties() const;
  static std::vector<uint8_t> encode_properties(
      const std::string& player_path,
      const std::map<std::string, sdbus::Variant>& properties);

 private:
  static constexpr auto kBluezService = "org.bluez";
  static constexpr auto kMediaPlayerIface = "org.bluez.MediaPlayer1";

  std::string player_path_;
  std::unique_ptr<sdbus::IProxy> proxy_;
};
