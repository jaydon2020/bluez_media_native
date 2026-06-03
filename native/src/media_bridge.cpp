#include "media_bridge.h"

#include <utility>

namespace {

std::string safe_string(const char *value) {
  return value == nullptr ? std::string{} : std::string{value};
}

bool as_bool(uint8_t value) { return value != 0; }

} // namespace

MediaBridge::MediaBridge(sdbus::IConnection &conn) : conn_(conn) {}

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
  object->emitInterfacesAddedSignal();

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

  it->second.object->emitInterfacesRemovedSignal();
  players_.erase(it);
  return 0;
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
