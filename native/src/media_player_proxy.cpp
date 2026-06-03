// media_player_proxy.cpp
#include "media_player_proxy.h"
#include "bluez_media_types.h"
#include "media_utils.h"

MediaPlayerProxy::MediaPlayerProxy(sdbus::IConnection &conn, const std::string &player_path)
    : player_path_(player_path) {
  if (player_path_.empty()) {
    throw sdbus::Error{sdbus::Error::Name{"org.bluez.Error.InvalidArguments"},
                       "player_path is required"};
  }
  proxy_ = sdbus::createProxy(conn, sdbus::ServiceName{kBluezService},
                              sdbus::ObjectPath{player_path_});
}

int MediaPlayerProxy::play() const {
  proxy_->callMethod("Play").onInterface(kMediaPlayerIface);
  return 0;
}

int MediaPlayerProxy::pause() const {
  proxy_->callMethod("Pause").onInterface(kMediaPlayerIface);
  return 0;
}

int MediaPlayerProxy::stop() const {
  proxy_->callMethod("Stop").onInterface(kMediaPlayerIface);
  return 0;
}

int MediaPlayerProxy::next() const {
  proxy_->callMethod("Next").onInterface(kMediaPlayerIface);
  return 0;
}

int MediaPlayerProxy::previous() const {
  proxy_->callMethod("Previous").onInterface(kMediaPlayerIface);
  return 0;
}

std::vector<uint8_t> MediaPlayerProxy::properties() const {
  BlueZMediaPlayerProps props;
  props.objectPath = player_path_;
  try {
    props.equalizer = proxy_->getProperty("Equalizer")
                          .onInterface(kMediaPlayerIface)
                          .get<std::string>();
  } catch (const sdbus::Error &) {}
  try {
    props.repeat = proxy_->getProperty("Repeat")
                       .onInterface(kMediaPlayerIface)
                       .get<std::string>();
  } catch (const sdbus::Error &) {}
  try {
    props.shuffle = proxy_->getProperty("Shuffle")
                        .onInterface(kMediaPlayerIface)
                        .get<std::string>();
  } catch (const sdbus::Error &) {}
  try {
    props.scan = proxy_->getProperty("Scan")
                     .onInterface(kMediaPlayerIface)
                     .get<std::string>();
  } catch (const sdbus::Error &) {}
  props.status = proxy_->getProperty("Status")
                     .onInterface(kMediaPlayerIface)
                     .get<std::string>();
  props.position = proxy_->getProperty("Position")
                       .onInterface(kMediaPlayerIface)
                       .get<uint32_t>();
  props.track = track_to_properties(
      proxy_->getProperty("Track")
          .onInterface(kMediaPlayerIface)
          .get<std::map<std::string, sdbus::Variant>>());

  try {
    props.device = proxy_->getProperty("Device")
                       .onInterface(kMediaPlayerIface)
                       .get<sdbus::ObjectPath>();
  } catch (const sdbus::Error &) {}
  try {
    props.name = proxy_->getProperty("Name")
                     .onInterface(kMediaPlayerIface)
                     .get<std::string>();
  } catch (const sdbus::Error &) {}
  try {
    props.type = proxy_->getProperty("Type")
                     .onInterface(kMediaPlayerIface)
                     .get<std::string>();
  } catch (const sdbus::Error &) {}
  try {
    props.subtype = proxy_->getProperty("Subtype")
                        .onInterface(kMediaPlayerIface)
                        .get<std::string>();
  } catch (const sdbus::Error &) {}
  try {
    props.browsable = proxy_->getProperty("Browsable")
                          .onInterface(kMediaPlayerIface)
                          .get<bool>();
  } catch (const sdbus::Error &) {}
  try {
    props.searchable = proxy_->getProperty("Searchable")
                           .onInterface(kMediaPlayerIface)
                           .get<bool>();
  } catch (const sdbus::Error &) {}
  try {
    props.playlist = proxy_->getProperty("Playlist")
                         .onInterface(kMediaPlayerIface)
                         .get<sdbus::ObjectPath>();
  } catch (const sdbus::Error &) {}

  return glz::encode(props);
}
