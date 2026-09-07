// media_transport_proxy.cpp
#include "media_transport_proxy.h"
#include "bluez_media_native.h"
#include "bluez_media_types.h"
#include "media_utils.h"

#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <system_error>

namespace {
int take_fd(sdbus::UnixFd& fd) {
  const int result = fd.release();
  if (result < 0 || fcntl(result, F_SETFD, FD_CLOEXEC) != 0) {
    const int error = result < 0 ? EBADF : errno;
    if (result >= 0) {
      close(result);
    }
    throw std::system_error(error, std::generic_category(),
                            "Failed to own transport file descriptor");
  }
  return result;
}
}  // namespace

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

BlueZMediaAcquireResult MediaTransportProxy::acquire() const {
  sdbus::UnixFd fd;
  uint16_t read_mtu = 0;
  uint16_t write_mtu = 0;
  media_call(*proxy_, kMediaTransportIface, "Acquire") >> fd >> read_mtu >> write_mtu;

  BlueZMediaAcquireResult result;
  result.transportPath = transport_path_;
  result.fd = take_fd(fd);
  result.readMtu = read_mtu;
  result.writeMtu = write_mtu;
  return result;
}

BlueZMediaAcquireResult MediaTransportProxy::try_acquire() const {
  sdbus::UnixFd fd;
  uint16_t read_mtu = 0;
  uint16_t write_mtu = 0;
  media_call(*proxy_, kMediaTransportIface, "TryAcquire") >> fd >> read_mtu >> write_mtu;

  BlueZMediaAcquireResult result;
  result.transportPath = transport_path_;
  result.fd = take_fd(fd);
  result.readMtu = read_mtu;
  result.writeMtu = write_mtu;
  return result;
}

int MediaTransportProxy::release() const {
  media_call(*proxy_, kMediaTransportIface, "Release");
  return BLUEZ_MEDIA_SUCCESS;
}

std::vector<uint8_t> MediaTransportProxy::properties() const {
  std::map<std::string, sdbus::Variant> properties;
  media_call(*proxy_, "org.freedesktop.DBus.Properties", "GetAll", std::string{kMediaTransportIface}) >> properties;
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
  media_call(*proxy_, "org.freedesktop.DBus.Properties", "Set",
      std::string{kMediaTransportIface}, std::string{"Volume"}, sdbus::Variant{volume});
  return BLUEZ_MEDIA_SUCCESS;
}
