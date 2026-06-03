#include "media_bridge.h"

#include <sstream>
#include <utility>

namespace {

std::string safe_string(const char *value) {
  return value == nullptr ? std::string{} : std::string{value};
}

bool as_bool(uint8_t value) { return value != 0; }

} // namespace

MediaBridge::MediaBridge(sdbus::IConnection &conn) : conn_(conn) {
  try {
    conn_.addObjectManager(sdbus::ObjectPath{"/"});
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "MediaBridge: failed to add ObjectManager: %s\n", e.what());
  }
}

int MediaBridge::register_player(
    const BluezMediaPlayerRegistration &registration) {
  auto state = make_state(registration);
  if (state.adapter_path.empty() || state.player_path.empty()) {
    return -1;
  }
  if (players_.contains(state.player_path)) {
    return -2;
  }

  auto object =
      sdbus::createObject(conn_, sdbus::ObjectPath{state.player_path});
  add_mpris_root_vtable(*object, state);
  add_mpris_player_vtable(*object, state);

  auto media_proxy =
      sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                         sdbus::ObjectPath{state.adapter_path});
  media_proxy->callMethod("RegisterPlayer")
      .onInterface(kMediaIface)
      .withArguments(sdbus::ObjectPath{state.player_path},
                     make_player_properties(state));

  players_.emplace(state.player_path, MediaPlayerRegistrationRecord{
                                          std::move(state), std::move(object)});
  return 0;
}

int MediaBridge::unregister_player(const char *adapter_path,
                                   const char *player_path) {
  const auto adapter = safe_string(adapter_path);
  const auto player = safe_string(player_path);
  if (adapter.empty() || player.empty()) {
    return -1;
  }

  auto it = players_.find(player);
  if (it == players_.end()) {
    return -2;
  }

  auto media_proxy = sdbus::createProxy(
      conn_, sdbus::ServiceName{kBluezService}, sdbus::ObjectPath{adapter});
  media_proxy->callMethod("UnregisterPlayer")
      .onInterface(kMediaIface)
      .withArguments(sdbus::ObjectPath{player});

  try {
    it->second.object->emitInterfacesRemovedSignal();
  } catch (const sdbus::Error &) {
  }
  players_.erase(it);
  return 0;
}

int MediaBridge::player_play(const char *player_path) {
  make_player_proxy(player_path)
      ->callMethod("Play")
      .onInterface(kMediaPlayerIface);
  return 0;
}

int MediaBridge::player_pause(const char *player_path) {
  make_player_proxy(player_path)
      ->callMethod("Pause")
      .onInterface(kMediaPlayerIface);
  return 0;
}

int MediaBridge::player_stop(const char *player_path) {
  make_player_proxy(player_path)
      ->callMethod("Stop")
      .onInterface(kMediaPlayerIface);
  return 0;
}

int MediaBridge::player_next(const char *player_path) {
  make_player_proxy(player_path)
      ->callMethod("Next")
      .onInterface(kMediaPlayerIface);
  return 0;
}

int MediaBridge::player_previous(const char *player_path) {
  make_player_proxy(player_path)
      ->callMethod("Previous")
      .onInterface(kMediaPlayerIface);
  return 0;
}

std::vector<uint8_t> MediaBridge::player_properties(const char *player_path) {
  const auto player = safe_string(player_path);
  if (player.empty()) {
    return {};
  }

  auto proxy = make_player_proxy(player_path);
  BlueZMediaPlayerProps props;
  props.objectPath = player;
  try {
    props.equalizer = proxy->getProperty("Equalizer")
                          .onInterface(kMediaPlayerIface)
                          .get<std::string>();
  } catch (const sdbus::Error &) {
  }
  try {
    props.repeat = proxy->getProperty("Repeat")
                       .onInterface(kMediaPlayerIface)
                       .get<std::string>();
  } catch (const sdbus::Error &) {
  }
  try {
    props.shuffle = proxy->getProperty("Shuffle")
                        .onInterface(kMediaPlayerIface)
                        .get<std::string>();
  } catch (const sdbus::Error &) {
  }
  try {
    props.scan = proxy->getProperty("Scan")
                     .onInterface(kMediaPlayerIface)
                     .get<std::string>();
  } catch (const sdbus::Error &) {
  }
  props.status = proxy->getProperty("Status")
                     .onInterface(kMediaPlayerIface)
                     .get<std::string>();
  props.position = proxy->getProperty("Position")
                       .onInterface(kMediaPlayerIface)
                       .get<uint32_t>();
  props.track =
      track_to_properties(proxy->getProperty("Track")
                              .onInterface(kMediaPlayerIface)
                              .get<std::map<std::string, sdbus::Variant>>());

  try {
    props.device = proxy->getProperty("Device")
                       .onInterface(kMediaPlayerIface)
                       .get<sdbus::ObjectPath>();
  } catch (const sdbus::Error &) {
  }
  try {
    props.name = proxy->getProperty("Name")
                     .onInterface(kMediaPlayerIface)
                     .get<std::string>();
  } catch (const sdbus::Error &) {
  }
  try {
    props.type = proxy->getProperty("Type")
                     .onInterface(kMediaPlayerIface)
                     .get<std::string>();
  } catch (const sdbus::Error &) {
  }
  try {
    props.subtype = proxy->getProperty("Subtype")
                        .onInterface(kMediaPlayerIface)
                        .get<std::string>();
  } catch (const sdbus::Error &) {
  }
  try {
    props.browsable = proxy->getProperty("Browsable")
                          .onInterface(kMediaPlayerIface)
                          .get<bool>();
  } catch (const sdbus::Error &) {
  }
  try {
    props.searchable = proxy->getProperty("Searchable")
                           .onInterface(kMediaPlayerIface)
                           .get<bool>();
  } catch (const sdbus::Error &) {
  }
  try {
    props.playlist = proxy->getProperty("Playlist")
                         .onInterface(kMediaPlayerIface)
                         .get<sdbus::ObjectPath>();
  } catch (const sdbus::Error &) {
  }

  return glz::encode(props);
}

int MediaBridge::control_play(const char *control_path) {
  make_control_proxy(control_path)
      ->callMethod("Play")
      .onInterface(kMediaControlIface);
  return 0;
}

int MediaBridge::control_pause(const char *control_path) {
  make_control_proxy(control_path)
      ->callMethod("Pause")
      .onInterface(kMediaControlIface);
  return 0;
}

int MediaBridge::control_stop(const char *control_path) {
  make_control_proxy(control_path)
      ->callMethod("Stop")
      .onInterface(kMediaControlIface);
  return 0;
}

int MediaBridge::control_next(const char *control_path) {
  make_control_proxy(control_path)
      ->callMethod("Next")
      .onInterface(kMediaControlIface);
  return 0;
}

int MediaBridge::control_previous(const char *control_path) {
  make_control_proxy(control_path)
      ->callMethod("Previous")
      .onInterface(kMediaControlIface);
  return 0;
}

int MediaBridge::control_volume_up(const char *control_path) {
  make_control_proxy(control_path)
      ->callMethod("VolumeUp")
      .onInterface(kMediaControlIface);
  return 0;
}

int MediaBridge::control_volume_down(const char *control_path) {
  make_control_proxy(control_path)
      ->callMethod("VolumeDown")
      .onInterface(kMediaControlIface);
  return 0;
}

int MediaBridge::control_fast_forward(const char *control_path) {
  make_control_proxy(control_path)
      ->callMethod("FastForward")
      .onInterface(kMediaControlIface);
  return 0;
}

int MediaBridge::control_rewind(const char *control_path) {
  make_control_proxy(control_path)
      ->callMethod("Rewind")
      .onInterface(kMediaControlIface);
  return 0;
}

std::vector<uint8_t> MediaBridge::control_properties(const char *control_path) {
  const auto control = safe_string(control_path);
  if (control.empty()) {
    return {};
  }

  auto proxy = make_control_proxy(control_path);
  BlueZMediaControlProps props;
  props.objectPath = control;
  props.connected = proxy->getProperty("Connected")
                        .onInterface(kMediaControlIface)
                        .get<bool>();
  try {
    props.player = proxy->getProperty("Player")
                       .onInterface(kMediaControlIface)
                       .get<sdbus::ObjectPath>();
  } catch (const sdbus::Error &) {
  }

  return glz::encode(props);
}

std::vector<uint8_t> MediaBridge::folder_search(const char *folder_path,
                                                const char *value) {
  const auto folder = safe_string(folder_path);
  if (folder.empty()) {
    return {};
  }

  auto proxy = make_folder_proxy(folder_path);
  sdbus::ObjectPath result;
  proxy->callMethod("Search")
      .onInterface(kMediaFolderIface)
      .withArguments(safe_string(value),
                     std::map<std::string, sdbus::Variant>{})
      .storeResultsTo(result);

  BlueZMediaFolderProps props;
  props.objectPath = result;
  return glz::encode(props);
}

std::vector<uint8_t> MediaBridge::folder_list_items(const char *folder_path) {
  const auto folder = safe_string(folder_path);
  if (folder.empty()) {
    return {};
  }

  auto proxy = make_folder_proxy(folder_path);
  std::map<sdbus::ObjectPath, std::map<std::string, sdbus::Variant>> result;
  proxy->callMethod("ListItems")
      .onInterface(kMediaFolderIface)
      .withArguments(std::map<std::string, sdbus::Variant>{})
      .storeResultsTo(result);

  BlueZMediaFolderItems items;
  items.objectPath = folder;
  items.items.reserve(result.size());
  for (const auto &[path, props] : result) {
    items.items.push_back(item_from_properties(path, props));
  }
  return glz::encode(items);
}

int MediaBridge::folder_change_folder(const char *folder_path,
                                      const char *target_folder_path) {
  const auto target = safe_string(target_folder_path);
  if (target.empty()) {
    return -1;
  }

  make_folder_proxy(folder_path)
      ->callMethod("ChangeFolder")
      .onInterface(kMediaFolderIface)
      .withArguments(sdbus::ObjectPath{target});
  return 0;
}

std::vector<uint8_t> MediaBridge::folder_properties(const char *folder_path) {
  const auto folder = safe_string(folder_path);
  if (folder.empty()) {
    return {};
  }

  auto proxy = make_folder_proxy(folder_path);
  BlueZMediaFolderProps props;
  props.objectPath = folder;
  props.numberOfItems = proxy->getProperty("NumberOfItems")
                            .onInterface(kMediaFolderIface)
                            .get<uint32_t>();
  props.name = proxy->getProperty("Name")
                   .onInterface(kMediaFolderIface)
                   .get<std::string>();
  return glz::encode(props);
}

int MediaBridge::item_play(const char *item_path) {
  make_item_proxy(item_path)->callMethod("Play").onInterface(kMediaItemIface);
  return 0;
}

int MediaBridge::item_add_to_now_playing(const char *item_path) {
  make_item_proxy(item_path)
      ->callMethod("AddtoNowPlaying")
      .onInterface(kMediaItemIface);
  return 0;
}

std::vector<uint8_t> MediaBridge::item_properties(const char *item_path) {
  const auto item = safe_string(item_path);
  if (item.empty()) {
    return {};
  }

  auto proxy = make_item_proxy(item_path);
  BlueZMediaItemProps props;
  props.objectPath = item;
  props.player = proxy->getProperty("Player")
                     .onInterface(kMediaItemIface)
                     .get<sdbus::ObjectPath>();
  props.name = proxy->getProperty("Name")
                   .onInterface(kMediaItemIface)
                   .get<std::string>();
  props.type = proxy->getProperty("Type")
                   .onInterface(kMediaItemIface)
                   .get<std::string>();
  props.folderType = proxy->getProperty("FolderType")
                         .onInterface(kMediaItemIface)
                         .get<std::string>();
  props.playable =
      proxy->getProperty("Playable").onInterface(kMediaItemIface).get<bool>();
  props.metadata =
      track_to_properties(proxy->getProperty("Metadata")
                              .onInterface(kMediaItemIface)
                              .get<std::map<std::string, sdbus::Variant>>());
  return glz::encode(props);
}

bool MediaBridge::has_player(const std::string &player_path) const {
  return players_.contains(player_path);
}

size_t MediaBridge::registered_player_count() const { return players_.size(); }

MediaPlayerState
MediaBridge::make_state(const BluezMediaPlayerRegistration &registration) {
  MediaPlayerState state;
  state.adapter_path = safe_string(registration.adapter_path);
  state.player_path = safe_string(registration.player_path);
  state.identity = safe_string(registration.identity);
  state.name = safe_string(registration.name);
  state.type = safe_string(registration.type);
  state.subtype = safe_string(registration.subtype);
  state.status = safe_string(registration.status);
  state.position_ms = registration.position_ms;
  state.can_go_next = as_bool(registration.can_go_next);
  state.can_go_previous = as_bool(registration.can_go_previous);
  state.can_play = as_bool(registration.can_play);
  state.can_pause = as_bool(registration.can_pause);
  state.can_seek = as_bool(registration.can_seek);
  state.can_control = as_bool(registration.can_control);
  state.browsable = as_bool(registration.browsable);
  state.searchable = as_bool(registration.searchable);

  if (state.identity.empty()) {
    state.identity = "bluez_media_native";
  }
  if (state.name.empty()) {
    state.name = state.identity;
  }
  if (state.type.empty()) {
    state.type = "audio";
  }
  if (state.status.empty()) {
    state.status = "stopped";
  }

  return state;
}

void MediaBridge::add_mpris_root_vtable(sdbus::IObject &object,
                                        const MediaPlayerState &state) {
  object
      .addVTable(
          sdbus::registerProperty("CanQuit").withGetter([]() { return false; }),
          sdbus::registerProperty("CanRaise").withGetter([]() {
            return false;
          }),
          sdbus::registerProperty("HasTrackList").withGetter([]() {
            return false;
          }),
          sdbus::registerProperty("Identity")
              .withGetter([identity = state.identity]() { return identity; }),
          sdbus::registerProperty("DesktopEntry").withGetter([]() {
            return std::string{"bluez_media_native"};
          }),
          sdbus::registerProperty("SupportedUriSchemes").withGetter([]() {
            return std::vector<std::string>{};
          }),
          sdbus::registerProperty("SupportedMimeTypes").withGetter([]() {
            return std::vector<std::string>{};
          }),
          sdbus::registerMethod("Raise").implementedAs([]() {}),
          sdbus::registerMethod("Quit").implementedAs([]() {}))
      .forInterface(kMprisRootIface);
}

void MediaBridge::add_mpris_player_vtable(sdbus::IObject &object,
                                          const MediaPlayerState &state) {
  object
      .addVTable(
          sdbus::registerMethod("Next").implementedAs([]() {}),
          sdbus::registerMethod("Previous").implementedAs([]() {}),
          sdbus::registerMethod("Pause").implementedAs([]() {}),
          sdbus::registerMethod("PlayPause").implementedAs([]() {}),
          sdbus::registerMethod("Stop").implementedAs([]() {}),
          sdbus::registerMethod("Play").implementedAs([]() {}),
          sdbus::registerMethod("Seek").implementedAs([](int64_t) {}),
          sdbus::registerMethod("SetPosition")
              .implementedAs([](const sdbus::ObjectPath &, int64_t) {}),
          sdbus::registerMethod("OpenUri").implementedAs(
              [](const std::string &) {}),
          sdbus::registerProperty("PlaybackStatus")
              .withGetter([status = state.status]() { return status; }),
          sdbus::registerProperty("LoopStatus")
              .withGetter([]() { return std::string{"None"}; })
              .withSetter([](const std::string &) {}),
          sdbus::registerProperty("Rate")
              .withGetter([]() { return 1.0; })
              .withSetter([](const double &) {}),
          sdbus::registerProperty("Shuffle")
              .withGetter([]() { return false; })
              .withSetter([](const bool &) {}),
          sdbus::registerProperty("Metadata").withGetter([]() {
            return std::map<std::string, sdbus::Variant>{};
          }),
          sdbus::registerProperty("Volume")
              .withGetter([]() { return 1.0; })
              .withSetter([](const double &) {}),
          sdbus::registerProperty("Position")
              .withGetter([position = state.position_ms]() {
                return static_cast<int64_t>(position) * 1000;
              }),
          sdbus::registerProperty("MinimumRate").withGetter([]() {
            return 1.0;
          }),
          sdbus::registerProperty("MaximumRate").withGetter([]() {
            return 1.0;
          }),
          sdbus::registerProperty("CanGoNext")
              .withGetter([value = state.can_go_next]() { return value; }),
          sdbus::registerProperty("CanGoPrevious")
              .withGetter([value = state.can_go_previous]() { return value; }),
          sdbus::registerProperty("CanPlay").withGetter(
              [value = state.can_play]() { return value; }),
          sdbus::registerProperty("CanPause")
              .withGetter([value = state.can_pause]() { return value; }),
          sdbus::registerProperty("CanSeek").withGetter(
              [value = state.can_seek]() { return value; }),
          sdbus::registerProperty("CanControl")
              .withGetter([value = state.can_control]() { return value; }))
      .forInterface(kMprisPlayerIface);
}

std::map<std::string, sdbus::Variant>
MediaBridge::make_player_properties(const MediaPlayerState &state) {
  std::map<std::string, sdbus::Variant> properties;
  properties["Name"] = sdbus::Variant{state.name};
  properties["Type"] = sdbus::Variant{state.type};
  properties["Subtype"] = sdbus::Variant{state.subtype};
  properties["Browsable"] = sdbus::Variant{state.browsable};
  properties["Searchable"] = sdbus::Variant{state.searchable};
  return properties;
}

std::string MediaBridge::variant_to_string(const sdbus::Variant &value) {
  if (value.containsValueOfType<std::string>()) {
    return value.get<std::string>();
  }
  if (value.containsValueOfType<sdbus::ObjectPath>()) {
    return value.get<sdbus::ObjectPath>();
  }
  if (value.containsValueOfType<bool>()) {
    return value.get<bool>() ? "true" : "false";
  }
  if (value.containsValueOfType<uint8_t>()) {
    return std::to_string(value.get<uint8_t>());
  }
  if (value.containsValueOfType<uint16_t>()) {
    return std::to_string(value.get<uint16_t>());
  }
  if (value.containsValueOfType<uint32_t>()) {
    return std::to_string(value.get<uint32_t>());
  }
  if (value.containsValueOfType<uint64_t>()) {
    return std::to_string(value.get<uint64_t>());
  }
  if (value.containsValueOfType<int16_t>()) {
    return std::to_string(value.get<int16_t>());
  }
  if (value.containsValueOfType<int32_t>()) {
    return std::to_string(value.get<int32_t>());
  }
  if (value.containsValueOfType<int64_t>()) {
    return std::to_string(value.get<int64_t>());
  }
  if (value.containsValueOfType<double>()) {
    std::ostringstream out;
    out << value.get<double>();
    return out.str();
  }
  if (value.containsValueOfType<std::vector<std::string>>()) {
    const auto values = value.get<std::vector<std::string>>();
    std::string joined;
    for (const auto &item : values) {
      if (!joined.empty()) {
        joined += ",";
      }
      joined += item;
    }
    return joined;
  }
  return value.dumpToString();
}

std::vector<BlueZMediaProperty> MediaBridge::track_to_properties(
    const std::map<std::string, sdbus::Variant> &track) {
  std::vector<BlueZMediaProperty> properties;
  properties.reserve(track.size());
  for (const auto &[key, value] : track) {
    properties.push_back(BlueZMediaProperty{key, variant_to_string(value)});
  }
  return properties;
}

BlueZMediaItemProps MediaBridge::item_from_properties(
    const std::string &object_path,
    const std::map<std::string, sdbus::Variant> &props) {
  BlueZMediaItemProps item;
  item.objectPath = object_path;
  if (const auto it = props.find("Player"); it != props.end()) {
    item.player = variant_to_string(it->second);
  }
  if (const auto it = props.find("Name"); it != props.end()) {
    item.name = variant_to_string(it->second);
  }
  if (const auto it = props.find("Type"); it != props.end()) {
    item.type = variant_to_string(it->second);
  }
  if (const auto it = props.find("FolderType"); it != props.end()) {
    item.folderType = variant_to_string(it->second);
  }
  if (const auto it = props.find("Playable");
      it != props.end() && it->second.containsValueOfType<bool>()) {
    item.playable = it->second.get<bool>();
  }
  if (const auto it = props.find("Metadata");
      it != props.end() &&
      it->second.containsValueOfType<std::map<std::string, sdbus::Variant>>()) {
    item.metadata = track_to_properties(
        it->second.get<std::map<std::string, sdbus::Variant>>());
  }
  return item;
}

std::unique_ptr<sdbus::IProxy>
MediaBridge::make_player_proxy(const char *player_path) const {
  const auto player = safe_string(player_path);
  if (player.empty()) {
    throw sdbus::Error{sdbus::Error::Name{"org.bluez.Error.InvalidArguments"},
                       "player_path is required"};
  }
  return sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                            sdbus::ObjectPath{player});
}

std::unique_ptr<sdbus::IProxy>
MediaBridge::make_control_proxy(const char *control_path) const {
  const auto control = safe_string(control_path);
  if (control.empty()) {
    throw sdbus::Error{sdbus::Error::Name{"org.bluez.Error.InvalidArguments"},
                       "control_path is required"};
  }
  return sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                            sdbus::ObjectPath{control});
}

std::unique_ptr<sdbus::IProxy>
MediaBridge::make_folder_proxy(const char *folder_path) const {
  const auto folder = safe_string(folder_path);
  if (folder.empty()) {
    throw sdbus::Error{sdbus::Error::Name{"org.bluez.Error.InvalidArguments"},
                       "folder_path is required"};
  }
  return sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                            sdbus::ObjectPath{folder});
}

std::unique_ptr<sdbus::IProxy>
MediaBridge::make_item_proxy(const char *item_path) const {
  const auto item = safe_string(item_path);
  if (item.empty()) {
    throw sdbus::Error{sdbus::Error::Name{"org.bluez.Error.InvalidArguments"},
                       "item_path is required"};
  }
  return sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                            sdbus::ObjectPath{item});
}
