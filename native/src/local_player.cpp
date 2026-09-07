// local_player.cpp
#include "local_player.h"
#include "media_utils.h"

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

  const auto properties = make_player_properties();
  register_mpris_object();
  auto media_proxy =
      sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                         sdbus::ObjectPath{state_.adapter_path});
  media_call(*media_proxy, kMediaIface, "RegisterPlayer", sdbus::ObjectPath{state_.player_path},
                     properties);
}

void LocalPlayer::register_mpris_object() {
  object_ = sdbus::createObject(conn_, sdbus::ObjectPath{state_.player_path});

  // Registration currently exports an inert MPRIS object. Never acknowledge
  // playback commands or advertise capabilities without an application bridge.
  const auto unsupported = [] {
    throw sdbus::Error{sdbus::Error::Name{"org.freedesktop.DBus.Error.NotSupported"},
                      "Local playback command routing is not implemented"};
  };
  object_->addVTable(
      sdbus::registerMethod("Next").implementedAs(unsupported),
      sdbus::registerMethod("Previous").implementedAs(unsupported),
      sdbus::registerMethod("Pause").implementedAs(unsupported),
      sdbus::registerMethod("PlayPause").implementedAs(unsupported),
      sdbus::registerMethod("Stop").implementedAs(unsupported),
      sdbus::registerMethod("Play").implementedAs(unsupported),
      sdbus::registerMethod("Seek").implementedAs([unsupported](int64_t) { unsupported(); }),
      sdbus::registerMethod("SetPosition").implementedAs(
          [unsupported](const sdbus::ObjectPath&, int64_t) { unsupported(); }),
      sdbus::registerMethod("OpenUri").implementedAs(
          [unsupported](const std::string&) { unsupported(); }),
      sdbus::registerProperty("PlaybackStatus").withGetter([] { return std::string{"Stopped"}; }),
      sdbus::registerProperty("LoopStatus").withGetter([] { return std::string{"None"}; })
          .withSetter([unsupported](const std::string&) { unsupported(); }),
      sdbus::registerProperty("Rate").withGetter([] { return 1.0; })
          .withSetter([unsupported](double) { unsupported(); }),
      sdbus::registerProperty("Shuffle").withGetter([] { return false; })
          .withSetter([unsupported](bool) { unsupported(); }),
      sdbus::registerProperty("Metadata").withGetter([] { return std::map<std::string, sdbus::Variant>{}; }),
      sdbus::registerProperty("Volume").withGetter([] { return 1.0; })
          .withSetter([unsupported](double) { unsupported(); }),
      sdbus::registerProperty("Position").withGetter([] { return int64_t{0}; }),
      sdbus::registerProperty("MinimumRate").withGetter([] { return 1.0; }),
      sdbus::registerProperty("MaximumRate").withGetter([] { return 1.0; }),
      sdbus::registerProperty("CanGoNext").withGetter([] { return false; }),
      sdbus::registerProperty("CanGoPrevious").withGetter([] { return false; }),
      sdbus::registerProperty("CanPlay").withGetter([] { return false; }),
      sdbus::registerProperty("CanPause").withGetter([] { return false; }),
      sdbus::registerProperty("CanSeek").withGetter([] { return false; }),
      sdbus::registerProperty("CanControl").withGetter([] { return false; }))
      .forInterface(kMprisPlayerIface);
}

LocalPlayer::~LocalPlayer() {
  if (!registered_) return;
  try {
    auto media_proxy =
        sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                           sdbus::ObjectPath{state_.adapter_path});
    media_call(*media_proxy, kMediaIface, "UnregisterPlayer", sdbus::ObjectPath{state_.player_path});
  } catch (const sdbus::Error& error) {
    std::fprintf(stderr, "LocalPlayer: unregister failed: %s\n", error.what());
  }
}

std::map<std::string, sdbus::Variant> LocalPlayer::make_player_properties()
    const {
  std::map<std::string, sdbus::Variant> properties;
  properties["Identity"] = sdbus::Variant{state_.name};
  properties["PlaybackStatus"] = sdbus::Variant{playback_status_};
  properties["Position"] = sdbus::Variant{position_};
  properties["Metadata"] = sdbus::Variant{std::map<std::string, sdbus::Variant>{}};
  properties["LoopStatus"] = sdbus::Variant{loop_status_};
  properties["Shuffle"] = sdbus::Variant{shuffle_};
  for (const auto* capability : {"CanPlay", "CanPause", "CanSeek",
                                  "CanGoNext", "CanGoPrevious", "CanControl"}) {
    properties[capability] = sdbus::Variant{false};
  }
  return properties;
}
