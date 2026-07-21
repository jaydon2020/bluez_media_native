// media_transport_proxy.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class MediaTransportProxy {
 public:
  MediaTransportProxy(sdbus::IConnection& conn,
                      const std::string& transport_path);

  std::vector<uint8_t> acquire() const;
  std::vector<uint8_t> try_acquire() const;
  int release() const;
  std::vector<uint8_t> properties() const;
  int set_volume(uint16_t volume) const;
  static std::vector<uint8_t> encode_properties(
      const std::string& transport_path,
      const std::map<std::string, sdbus::Variant>& properties);

 private:
  static constexpr auto kBluezService = "org.bluez";
  static constexpr auto kMediaTransportIface = "org.bluez.MediaTransport1";

  std::string transport_path_;
  std::unique_ptr<sdbus::IProxy> proxy_;
};
