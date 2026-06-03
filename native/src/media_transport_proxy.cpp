// media_transport_proxy.cpp
#include "media_transport_proxy.h"
#include "bluez_media_types.h"
#include <unistd.h>

MediaTransportProxy::MediaTransportProxy(sdbus::IConnection &conn, const std::string &transport_path)
    : transport_path_(transport_path) {
  if (transport_path_.empty()) {
    throw sdbus::Error{sdbus::Error::Name{"org.bluez.Error.InvalidArguments"},
                       "transport_path is required"};
  }
  proxy_ = sdbus::createProxy(conn, sdbus::ServiceName{kBluezService},
                              sdbus::ObjectPath{transport_path_});
}

std::vector<uint8_t> MediaTransportProxy::acquire() const {
  sdbus::UnixFd fd;
  uint16_t read_mtu = 0;
  uint16_t write_mtu = 0;
  proxy_->callMethod("Acquire")
      .onInterface(kMediaTransportIface)
      .storeResultsTo(fd, read_mtu, write_mtu);

  BlueZMediaAcquireResult result;
  result.transportPath = transport_path_;
  result.fd = static_cast<uint64_t>(dup(fd.get()));
  result.readMtu = read_mtu;
  result.writeMtu = write_mtu;
  return glz::encode(result);
}

std::vector<uint8_t> MediaTransportProxy::try_acquire() const {
  sdbus::UnixFd fd;
  uint16_t read_mtu = 0;
  uint16_t write_mtu = 0;
  proxy_->callMethod("TryAcquire")
      .onInterface(kMediaTransportIface)
      .storeResultsTo(fd, read_mtu, write_mtu);

  BlueZMediaAcquireResult result;
  result.transportPath = transport_path_;
  result.fd = static_cast<uint64_t>(dup(fd.get()));
  result.readMtu = read_mtu;
  result.writeMtu = write_mtu;
  return glz::encode(result);
}

int MediaTransportProxy::release() const {
  proxy_->callMethod("Release").onInterface(kMediaTransportIface);
  return 0;
}

std::vector<uint8_t> MediaTransportProxy::properties() const {
  BlueZMediaTransportProps props;
  props.objectPath = transport_path_;
  try {
    props.device = proxy_->getProperty("Device")
                       .onInterface(kMediaTransportIface)
                       .get<sdbus::ObjectPath>();
  } catch (const sdbus::Error &) {}
  try {
    props.uuid = proxy_->getProperty("UUID")
                     .onInterface(kMediaTransportIface)
                     .get<std::string>();
  } catch (const sdbus::Error &) {}
  try {
    props.codec = proxy_->getProperty("Codec")
                      .onInterface(kMediaTransportIface)
                      .get<uint8_t>();
  } catch (const sdbus::Error &) {}
  try {
    props.configuration = proxy_->getProperty("Configuration")
                              .onInterface(kMediaTransportIface)
                              .get<std::vector<uint8_t>>();
  } catch (const sdbus::Error &) {}
  try {
    props.state = proxy_->getProperty("State")
                      .onInterface(kMediaTransportIface)
                      .get<std::string>();
  } catch (const sdbus::Error &) {}
  try {
    props.delay = proxy_->getProperty("Delay")
                      .onInterface(kMediaTransportIface)
                      .get<uint16_t>();
  } catch (const sdbus::Error &) {}
  try {
    props.volume = proxy_->getProperty("Volume")
                       .onInterface(kMediaTransportIface)
                       .get<uint16_t>();
  } catch (const sdbus::Error &) {}
  try {
    props.endpoint = proxy_->getProperty("Endpoint")
                         .onInterface(kMediaTransportIface)
                         .get<sdbus::ObjectPath>();
  } catch (const sdbus::Error &) {}
  return glz::encode(props);
}

int MediaTransportProxy::set_volume(uint16_t volume) const {
  proxy_->setProperty("Volume")
      .onInterface(kMediaTransportIface)
      .toValue(volume);
  return 0;
}
