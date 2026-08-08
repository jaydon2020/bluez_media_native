// media_player_proxy.cpp
#include "media_player_proxy.h"
#include "bluez_media_native.h"
#include "bluez_media_types.h"
#include "media_utils.h"

MediaPlayerProxy::MediaPlayerProxy(sdbus::IConnection& conn,
                                   const std::string& player_path)
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
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaPlayerProxy::pause() const {
  proxy_->callMethod("Pause").onInterface(kMediaPlayerIface);
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaPlayerProxy::stop() const {
  proxy_->callMethod("Stop").onInterface(kMediaPlayerIface);
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaPlayerProxy::next() const {
  proxy_->callMethod("Next").onInterface(kMediaPlayerIface);
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaPlayerProxy::previous() const {
  proxy_->callMethod("Previous").onInterface(kMediaPlayerIface);
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaPlayerProxy::fast_forward() const {
  proxy_->callMethod("FastForward").onInterface(kMediaPlayerIface);
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaPlayerProxy::rewind() const {
  proxy_->callMethod("Rewind").onInterface(kMediaPlayerIface);
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaPlayerProxy::set_repeat(const std::string& repeat) const {
  proxy_->setProperty("Repeat").onInterface(kMediaPlayerIface).toValue(repeat);
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaPlayerProxy::set_shuffle(const std::string& shuffle) const {
  proxy_->setProperty("Shuffle")
      .onInterface(kMediaPlayerIface)
      .toValue(shuffle);
  return BLUEZ_MEDIA_SUCCESS;
}

std::vector<uint8_t> MediaPlayerProxy::properties() const {
  std::map<std::string, sdbus::Variant> properties;
  proxy_->callMethod("GetAll")
      .onInterface("org.freedesktop.DBus.Properties")
      .withArguments(std::string{kMediaPlayerIface})
      .storeResultsTo(properties);
  return encode_properties(player_path_, properties);
}

std::vector<uint8_t> MediaPlayerProxy::encode_properties(
    const std::string& player_path,
    const std::map<std::string, sdbus::Variant>& properties) {
  BlueZMediaPlayerProps props;
  props.objectPath = player_path;
  props.equalizer = media_property<std::string>(properties, "Equalizer");
  props.repeat = media_property<std::string>(properties, "Repeat");
  props.shuffle = media_property<std::string>(properties, "Shuffle");
  props.scan = media_property<std::string>(properties, "Scan");
  props.status = media_property<std::string>(properties, "Status");
  props.position = media_property<uint32_t>(properties, "Position");
  props.track =
      track_to_properties(media_property<std::map<std::string, sdbus::Variant>>(
          properties, "Track"));
  props.device = media_property<sdbus::ObjectPath>(properties, "Device");
  props.name = media_property<std::string>(properties, "Name");
  props.type = media_property<std::string>(properties, "Type");
  props.subtype = media_property<std::string>(properties, "Subtype");
  props.browsable = media_property<bool>(properties, "Browsable");
  props.searchable = media_property<bool>(properties, "Searchable");
  props.playlist = media_property<sdbus::ObjectPath>(properties, "Playlist");

  return glz::encode(props);
}
