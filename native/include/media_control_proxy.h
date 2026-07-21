// media_control_proxy.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class MediaControlProxy {
 public:
  MediaControlProxy(sdbus::IConnection& conn, const std::string& control_path);

  int play() const;
  int pause() const;
  int stop() const;
  int next() const;
  int previous() const;
  int volume_up() const;
  int volume_down() const;
  int fast_forward() const;
  int rewind() const;
  std::vector<uint8_t> properties() const;
  static std::vector<uint8_t> encode_properties(
      const std::string& control_path,
      const std::map<std::string, sdbus::Variant>& properties);

 private:
  static constexpr auto kBluezService = "org.bluez";
  static constexpr auto kMediaControlIface = "org.bluez.MediaControl1";

  std::string control_path_;
  std::unique_ptr<sdbus::IProxy> proxy_;
};
