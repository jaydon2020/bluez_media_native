// media_transport_proxy.cpp
#include "media_transport_proxy.h"
#include "bluez_media_native.h"
#include "bluez_media_types.h"
#include "media_utils.h"

#include <unistd.h>
#include <cerrno>
#include <system_error>

MediaTransportProxy::MediaTransportProxy(sdbus::IConnection& conn,
                                         const std::string& transport_path)
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

  const int duplicated_fd = dup(fd.get());
  if (duplicated_fd < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to duplicate transport file descriptor");
  }

  BlueZMediaAcquireResult result;
  result.transportPath = transport_path_;
  result.fd = static_cast<uint64_t>(duplicated_fd);
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

  const int duplicated_fd = dup(fd.get());
  if (duplicated_fd < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to duplicate transport file descriptor");
  }

  BlueZMediaAcquireResult result;
  result.transportPath = transport_path_;
  result.fd = static_cast<uint64_t>(duplicated_fd);
  result.readMtu = read_mtu;
  result.writeMtu = write_mtu;
  return glz::encode(result);
}

int MediaTransportProxy::release() const {
  proxy_->callMethod("Release").onInterface(kMediaTransportIface);
  return BLUEZ_MEDIA_SUCCESS;
}

std::vector<uint8_t> MediaTransportProxy::properties() const {
  std::map<std::string, sdbus::Variant> properties;
  proxy_->callMethod("GetAll")
      .onInterface("org.freedesktop.DBus.Properties")
      .withArguments(std::string{kMediaTransportIface})
      .storeResultsTo(properties);
  return encode_properties(transport_path_, properties);
}

std::vector<uint8_t> MediaTransportProxy::encode_properties(
    const std::string& transport_path,
    const std::map<std::string, sdbus::Variant>& properties) {
  BlueZMediaTransportProps props;
  props.objectPath = transport_path;
  props.device = media_property<sdbus::ObjectPath>(properties, "Device");
  props.uuid = media_property<std::string>(properties, "UUID");
  props.codec = media_property<uint8_t>(properties, "Codec");
  props.configuration =
      media_property<std::vector<uint8_t>>(properties, "Configuration");
  props.state = media_property<std::string>(properties, "State");
  props.delay = media_property<uint16_t>(properties, "Delay");
  props.volume = media_property<uint16_t>(properties, "Volume");
  props.endpoint = media_property<sdbus::ObjectPath>(properties, "Endpoint");
  return glz::encode(props);
}

int MediaTransportProxy::set_volume(uint16_t volume) const {
  proxy_->setProperty("Volume")
      .onInterface(kMediaTransportIface)
      .toValue(volume);
  return BLUEZ_MEDIA_SUCCESS;
}
