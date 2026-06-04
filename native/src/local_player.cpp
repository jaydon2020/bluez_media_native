// local_player.cpp
#include "local_player.h"

namespace {
std::string safe_string(const char *value) {
  return value == nullptr ? std::string{} : std::string{value};
}
bool as_bool(uint8_t value) { return value != 0; }
} // namespace

LocalPlayer::LocalPlayer(sdbus::IConnection &conn,
                         const BluezMediaPlayerRegistration &registration)
    : conn_(conn) {
  state_.adapter_path = safe_string(registration.adapter_path);
  state_.player_path = safe_string(registration.player_path);
  state_.identity = safe_string(registration.identity);
  state_.name = safe_string(registration.name);
  state_.type = safe_string(registration.type);
  state_.subtype = safe_string(registration.subtype);
  state_.status = safe_string(registration.status);
  state_.position_ms = registration.position_ms;
  state_.can_go_next = as_bool(registration.can_go_next);
  state_.can_go_previous = as_bool(registration.can_go_previous);
  state_.can_play = as_bool(registration.can_play);
  state_.can_pause = as_bool(registration.can_pause);
  state_.can_seek = as_bool(registration.can_seek);
  state_.can_control = as_bool(registration.can_control);
  state_.browsable = as_bool(registration.browsable);
  state_.searchable = as_bool(registration.searchable);

  if (state_.identity.empty())
    state_.identity = "bluez_media_native";
  if (state_.name.empty())
    state_.name = state_.identity;
  if (state_.type.empty())
    state_.type = "audio";
  if (state_.status.empty())
    state_.status = "stopped";

  auto media_proxy =
      sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                         sdbus::ObjectPath{state_.adapter_path});
  media_proxy->callMethod("RegisterPlayer")
      .onInterface(kMediaIface)
      .withArguments(sdbus::ObjectPath{state_.player_path},
                     make_player_properties());
}

LocalPlayer::~LocalPlayer() {
  try {
    auto media_proxy =
        sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                           sdbus::ObjectPath{state_.adapter_path});
    media_proxy->callMethod("UnregisterPlayer")
        .onInterface(kMediaIface)
        .withArguments(sdbus::ObjectPath{state_.player_path});
  } catch (const sdbus::Error &) {
  }
}

std::map<std::string, sdbus::Variant>
LocalPlayer::make_player_properties() const {
  std::map<std::string, sdbus::Variant> properties;
  properties["Name"] = sdbus::Variant{state_.name};
  properties["Type"] = sdbus::Variant{state_.type};
  properties["Subtype"] = sdbus::Variant{state_.subtype};
  properties["Browsable"] = sdbus::Variant{state_.browsable};
  properties["Searchable"] = sdbus::Variant{state_.searchable};
  return properties;
}
