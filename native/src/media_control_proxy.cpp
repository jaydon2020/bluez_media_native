// media_control_proxy.cpp
#include "media_control_proxy.h"
#include "bluez_media_native.h"
#include "bluez_media_types.h"
#include "media_utils.h"

MediaControlProxy::MediaControlProxy(sdbus::IConnection& conn,
                                     const std::string& control_path)
    : control_path_(control_path) {
  if (control_path_.empty()) {
    throw sdbus::Error{sdbus::Error::Name{"org.bluez.Error.InvalidArguments"},
                       "control_path is required"};
  }
  proxy_ = sdbus::createProxy(conn, sdbus::ServiceName{kBluezService},
                              sdbus::ObjectPath{control_path_});
}

int MediaControlProxy::play() const {
  proxy_->callMethod("Play").onInterface(kMediaControlIface);
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaControlProxy::pause() const {
  proxy_->callMethod("Pause").onInterface(kMediaControlIface);
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaControlProxy::stop() const {
  proxy_->callMethod("Stop").onInterface(kMediaControlIface);
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaControlProxy::next() const {
  proxy_->callMethod("Next").onInterface(kMediaControlIface);
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaControlProxy::previous() const {
  proxy_->callMethod("Previous").onInterface(kMediaControlIface);
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaControlProxy::volume_up() const {
  proxy_->callMethod("VolumeUp").onInterface(kMediaControlIface);
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaControlProxy::volume_down() const {
  proxy_->callMethod("VolumeDown").onInterface(kMediaControlIface);
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaControlProxy::fast_forward() const {
  proxy_->callMethod("FastForward").onInterface(kMediaControlIface);
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaControlProxy::rewind() const {
  proxy_->callMethod("Rewind").onInterface(kMediaControlIface);
  return BLUEZ_MEDIA_SUCCESS;
}

std::vector<uint8_t> MediaControlProxy::properties() const {
  std::map<std::string, sdbus::Variant> properties;
  proxy_->callMethod("GetAll")
      .onInterface("org.freedesktop.DBus.Properties")
      .withArguments(std::string{kMediaControlIface})
      .storeResultsTo(properties);
  return encode_properties(control_path_, properties);
}

std::vector<uint8_t> MediaControlProxy::encode_properties(
    const std::string& control_path,
    const std::map<std::string, sdbus::Variant>& properties) {
  BlueZMediaControlProps props;
  props.objectPath = control_path;
  props.connected = media_property<bool>(properties, "Connected");
  props.player = media_property<sdbus::ObjectPath>(properties, "Player");

  return glz::encode(props);
}
