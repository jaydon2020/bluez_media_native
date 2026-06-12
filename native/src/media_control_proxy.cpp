// media_control_proxy.cpp
#include "media_control_proxy.h"
#include "bluez_media_native.h"
#include "bluez_media_types.h"

MediaControlProxy::MediaControlProxy(sdbus::IConnection &conn,
                                     const std::string &control_path)
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
  BlueZMediaControlProps props;
  props.objectPath = control_path_;
  try {
    props.connected = proxy_->getProperty("Connected")
                          .onInterface(kMediaControlIface)
                          .get<bool>();
  } catch (const sdbus::Error &) {
  }
  try {
    props.player = proxy_->getProperty("Player")
                       .onInterface(kMediaControlIface)
                       .get<sdbus::ObjectPath>();
  } catch (const sdbus::Error &) {
  }

  return glz::encode(props);
}
