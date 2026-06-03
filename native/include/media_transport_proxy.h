// media_transport_proxy.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <vector>
#include <cstdint>
#include <memory>
#include <string>

class MediaTransportProxy {
public:
  MediaTransportProxy(sdbus::IConnection &conn, const std::string &transport_path);

  std::vector<uint8_t> acquire() const;
  std::vector<uint8_t> try_acquire() const;
  int release() const;
  std::vector<uint8_t> properties() const;
  int set_volume(uint16_t volume) const;

private:
  static constexpr auto kBluezService = "org.bluez";
  static constexpr auto kMediaTransportIface = "org.bluez.MediaTransport1";

  std::string transport_path_;
  std::unique_ptr<sdbus::IProxy> proxy_;
};
