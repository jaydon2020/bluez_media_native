// media_control_proxy.cpp
#include "media_control_proxy.h"
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
  return 0;
}

int MediaControlProxy::pause() const {
  proxy_->callMethod("Pause").onInterface(kMediaControlIface);
  return 0;
}

int MediaControlProxy::stop() const {
  proxy_->callMethod("Stop").onInterface(kMediaControlIface);
  return 0;
}

int MediaControlProxy::next() const {
  proxy_->callMethod("Next").onInterface(kMediaControlIface);
  return 0;
}

int MediaControlProxy::previous() const {
  proxy_->callMethod("Previous").onInterface(kMediaControlIface);
  return 0;
}

int MediaControlProxy::volume_up() const {
  proxy_->callMethod("VolumeUp").onInterface(kMediaControlIface);
  return 0;
}

int MediaControlProxy::volume_down() const {
  proxy_->callMethod("VolumeDown").onInterface(kMediaControlIface);
  return 0;
}

int MediaControlProxy::fast_forward() const {
  proxy_->callMethod("FastForward").onInterface(kMediaControlIface);
  return 0;
}

int MediaControlProxy::rewind() const {
  proxy_->callMethod("Rewind").onInterface(kMediaControlIface);
  return 0;
}

std::vector<uint8_t> MediaControlProxy::properties() const {
  BlueZMediaControlProps props;
  props.objectPath = control_path_;
  props.connected = proxy_->getProperty("Connected")
                        .onInterface(kMediaControlIface)
                        .get<bool>();
  try {
    props.player = proxy_->getProperty("Player")
                       .onInterface(kMediaControlIface)
                       .get<sdbus::ObjectPath>();
  } catch (const sdbus::Error &) {
  }

  return glz::encode(props);
}
