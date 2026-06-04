// media_control_proxy.h
#pragma once

#include <cstdint>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <vector>

class MediaControlProxy {
public:
  MediaControlProxy(sdbus::IConnection &conn, const std::string &control_path);

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

private:
  static constexpr auto kBluezService = "org.bluez";
  static constexpr auto kMediaControlIface = "org.bluez.MediaControl1";

  std::string control_path_;
  std::unique_ptr<sdbus::IProxy> proxy_;
};
