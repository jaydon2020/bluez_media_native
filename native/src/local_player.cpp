// local_player.cpp
#include "local_player.h"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace {
std::string safe_string(const char* value) {
  return value == nullptr ? std::string{} : std::string{value};
}
}  // namespace

LocalPlayer::LocalPlayer(sdbus::IConnection& conn,
                         const BluezMediaPlayerRegistration& registration)
    : conn_(conn) {
  state_.adapter_path = safe_string(registration.adapter_path);
  state_.player_path = safe_string(registration.player_path);
  state_.name = safe_string(registration.name);
  state_.type = safe_string(registration.type);
  state_.subtype = safe_string(registration.subtype);
  if (state_.name.empty())
    state_.name = "bluez_media_native";
  if (state_.type.empty())
    state_.type = "Audio";

  register_mpris_object();
  auto media_proxy =
      sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                         sdbus::ObjectPath{state_.adapter_path});
  media_proxy->callMethod("RegisterPlayer")
      .onInterface(kMediaIface)
      .withArguments(sdbus::ObjectPath{state_.player_path},
                     make_player_properties());
}

void LocalPlayer::register_mpris_object() {
  object_ = sdbus::createObject(conn_, sdbus::ObjectPath{state_.player_path});

  object_
      ->addVTable(
          sdbus::registerMethod("Next").implementedAs(
              [this] { position_ = 0; }),
          sdbus::registerMethod("Previous").implementedAs([this] {
            position_ = 0;
          }),
          sdbus::registerMethod("Pause").implementedAs(
              [this] { set_playback_status("Paused"); }),
          sdbus::registerMethod("PlayPause").implementedAs([this] {
            set_playback_status(playback_status_ == "Playing" ? "Paused"
                                                              : "Playing");
          }),
          sdbus::registerMethod("Stop").implementedAs([this] {
            position_ = 0;
            set_playback_status("Stopped");
          }),
          sdbus::registerMethod("Play").implementedAs(
              [this] { set_playback_status("Playing"); }),
          sdbus::registerMethod("Seek")
              .withInputParamNames("Offset")
              .implementedAs([this](int64_t offset) {
                position_ = std::max<int64_t>(0, position_ + offset);
              }),
          sdbus::registerMethod("SetPosition")
              .withInputParamNames("TrackId", "Position")
              .implementedAs(
                  [this](const sdbus::ObjectPath&, int64_t position) {
                    position_ = std::max<int64_t>(0, position);
                  }),
          sdbus::registerMethod("OpenUri")
              .withInputParamNames("Uri")
              .implementedAs([](const std::string&) {}),
          sdbus::registerProperty("PlaybackStatus").withGetter([this] {
            return playback_status_;
          }),
          sdbus::registerProperty("LoopStatus")
              .withGetter([this] { return loop_status_; })
              .withSetter(
                  [this](const std::string& value) { loop_status_ = value; }),
          sdbus::registerProperty("Rate")
              .withGetter([this] { return rate_; })
              .withSetter([this](double value) { rate_ = value; }),
          sdbus::registerProperty("Shuffle")
              .withGetter([this] { return shuffle_; })
              .withSetter([this](bool value) { shuffle_ = value; }),
          sdbus::registerProperty("Metadata").withGetter([] {
            return std::map<std::string, sdbus::Variant>{};
          }),
          sdbus::registerProperty("Volume")
              .withGetter([this] { return volume_; })
              .withSetter([this](double value) {
                volume_ = std::clamp(value, 0.0, 1.0);
              }),
          sdbus::registerProperty("Position").withGetter([this] {
            return position_;
          }),
          sdbus::registerProperty("MinimumRate").withGetter([] { return 1.0; }),
          sdbus::registerProperty("MaximumRate").withGetter([] { return 1.0; }),
          sdbus::registerProperty("CanGoNext").withGetter([] { return true; }),
          sdbus::registerProperty("CanGoPrevious").withGetter([] {
            return true;
          }),
          sdbus::registerProperty("CanPlay").withGetter([] { return true; }),
          sdbus::registerProperty("CanPause").withGetter([] { return true; }),
          sdbus::registerProperty("CanSeek").withGetter([] { return true; }),
          sdbus::registerProperty("CanControl").withGetter([] { return true; }))
      .forInterface(kMprisPlayerIface);
}

void LocalPlayer::set_playback_status(std::string status) {
  playback_status_ = std::move(status);
  object_->emitPropertiesChangedSignal(
      kMprisPlayerIface,
      std::vector<sdbus::PropertyName>{sdbus::PropertyName{"PlaybackStatus"}});
}

LocalPlayer::~LocalPlayer() {
  try {
    auto media_proxy =
        sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                           sdbus::ObjectPath{state_.adapter_path});
    media_proxy->callMethod("UnregisterPlayer")
        .onInterface(kMediaIface)
        .withArguments(sdbus::ObjectPath{state_.player_path});
  } catch (const sdbus::Error& error) {
    std::fprintf(stderr, "LocalPlayer: unregister failed: %s\n", error.what());
  }
}

std::map<std::string, sdbus::Variant> LocalPlayer::make_player_properties()
    const {
  std::map<std::string, sdbus::Variant> properties;
  properties["Name"] = sdbus::Variant{state_.name};
  properties["Type"] = sdbus::Variant{state_.type};
  properties["Subtype"] = sdbus::Variant{state_.subtype};
  return properties;
}
