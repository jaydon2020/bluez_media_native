// bluez_media_types.h — wire types for BlueZ Media-to-Dart payloads.
//
// Message discriminator byte at offset 0 in kExternalTypedData:
//   0x01 = BlueZMediaPlayerProps     (player PropertiesChanged or snapshot)
//   0x02 = BlueZMediaControlProps    (control PropertiesChanged or snapshot)
//   0x03 = BlueZMediaEndpointProps   (endpoint configuration)
//   0x04 = BlueZMediaTransportProps  (transport PropertiesChanged or snapshot)
//   0x05 = BlueZMediaFolderProps     (folder PropertiesChanged or snapshot)
//   0x06 = BlueZMediaItemProps       (media item PropertiesChanged or snapshot)
//   0x7E = BlueZMediaObjectRemoved   (ObjectManager InterfacesRemoved)
//   0x10 = BlueZMediaAcquireResult   (Acquire / TryAcquire result)
//   0x20 = BlueZMediaError           (method call failed)
//   0xFF = sentinel (stream done)

#pragma once

#include "glaze_meta.h"

#include <cstdint>
#include <string>
#include <vector>

// ── Shared key/value type ──────────────────────────────────────────────────

struct BlueZMediaProperty {
  std::string key;
  std::string value;
};
template <> struct glz::meta<BlueZMediaProperty> {
  static constexpr auto fields =
      std::make_tuple(glz::field("key", &BlueZMediaProperty::key),
                      glz::field("value", &BlueZMediaProperty::value));
};

// ── MediaPlayer1 properties ────────────────────────────────────────────────

struct BlueZMediaPlayerProps {
  std::string objectPath;
  uint32_t changedMask{};
  std::string equalizer;
  std::string repeat;
  std::string shuffle;
  std::string scan;
  std::string status;
  uint32_t position{};
  std::vector<BlueZMediaProperty> track;
  std::string device;
  std::string name;
  std::string type;
  std::string subtype;
  bool browsable{};
  bool searchable{};
  std::string playlist;
};
template <> struct glz::meta<BlueZMediaPlayerProps> {
  static constexpr auto fields = std::make_tuple(
      glz::field("objectPath", &BlueZMediaPlayerProps::objectPath),
      glz::field("changedMask", &BlueZMediaPlayerProps::changedMask),
      glz::field("equalizer", &BlueZMediaPlayerProps::equalizer),
      glz::field("repeat", &BlueZMediaPlayerProps::repeat),
      glz::field("shuffle", &BlueZMediaPlayerProps::shuffle),
      glz::field("scan", &BlueZMediaPlayerProps::scan),
      glz::field("status", &BlueZMediaPlayerProps::status),
      glz::field("position", &BlueZMediaPlayerProps::position),
      glz::field("track", &BlueZMediaPlayerProps::track),
      glz::field("device", &BlueZMediaPlayerProps::device),
      glz::field("name", &BlueZMediaPlayerProps::name),
      glz::field("type", &BlueZMediaPlayerProps::type),
      glz::field("subtype", &BlueZMediaPlayerProps::subtype),
      glz::field("browsable", &BlueZMediaPlayerProps::browsable),
      glz::field("searchable", &BlueZMediaPlayerProps::searchable),
      glz::field("playlist", &BlueZMediaPlayerProps::playlist));
};

// ── MediaControl1 properties ───────────────────────────────────────────────

struct BlueZMediaControlProps {
  std::string objectPath;
  uint32_t changedMask{};
  bool connected{};
  std::string player;
};
template <> struct glz::meta<BlueZMediaControlProps> {
  static constexpr auto fields = std::make_tuple(
      glz::field("objectPath", &BlueZMediaControlProps::objectPath),
      glz::field("changedMask", &BlueZMediaControlProps::changedMask),
      glz::field("connected", &BlueZMediaControlProps::connected),
      glz::field("player", &BlueZMediaControlProps::player));
};

// ── MediaEndpoint1 properties/configuration ────────────────────────────────

struct BlueZMediaEndpointProps {
  std::string objectPath;
  std::string uuid;
  uint8_t codec{};
  std::vector<uint8_t> capabilities;
  std::string device;
  bool delayReporting{};
};
template <> struct glz::meta<BlueZMediaEndpointProps> {
  static constexpr auto fields = std::make_tuple(
      glz::field("objectPath", &BlueZMediaEndpointProps::objectPath),
      glz::field("uuid", &BlueZMediaEndpointProps::uuid),
      glz::field("codec", &BlueZMediaEndpointProps::codec),
      glz::field("capabilities", &BlueZMediaEndpointProps::capabilities),
      glz::field("device", &BlueZMediaEndpointProps::device),
      glz::field("delayReporting", &BlueZMediaEndpointProps::delayReporting));
};

// ── MediaTransport1 properties ─────────────────────────────────────────────

struct BlueZMediaTransportProps {
  std::string objectPath;
  std::string device;
  std::string uuid;
  uint8_t codec{};
  std::vector<uint8_t> configuration;
  std::string state;
  uint16_t delay{};
  uint16_t volume{};
  std::string endpoint;
};
template <> struct glz::meta<BlueZMediaTransportProps> {
  static constexpr auto fields = std::make_tuple(
      glz::field("objectPath", &BlueZMediaTransportProps::objectPath),
      glz::field("device", &BlueZMediaTransportProps::device),
      glz::field("uuid", &BlueZMediaTransportProps::uuid),
      glz::field("codec", &BlueZMediaTransportProps::codec),
      glz::field("configuration", &BlueZMediaTransportProps::configuration),
      glz::field("state", &BlueZMediaTransportProps::state),
      glz::field("delay", &BlueZMediaTransportProps::delay),
      glz::field("volume", &BlueZMediaTransportProps::volume),
      glz::field("endpoint", &BlueZMediaTransportProps::endpoint));
};

// ── MediaFolder1 properties ────────────────────────────────────────────────

struct BlueZMediaFolderProps {
  std::string objectPath;
  uint32_t numberOfItems{};
  std::string name;
};
template <> struct glz::meta<BlueZMediaFolderProps> {
  static constexpr auto fields = std::make_tuple(
      glz::field("objectPath", &BlueZMediaFolderProps::objectPath),
      glz::field("numberOfItems", &BlueZMediaFolderProps::numberOfItems),
      glz::field("name", &BlueZMediaFolderProps::name));
};

// ── MediaItem1 properties ──────────────────────────────────────────────────

struct BlueZMediaItemProps {
  std::string objectPath;
  std::string player;
  std::string name;
  std::string type;
  std::string folderType;
  bool playable{};
  std::vector<BlueZMediaProperty> metadata;
};
template <> struct glz::meta<BlueZMediaItemProps> {
  static constexpr auto fields = std::make_tuple(
      glz::field("objectPath", &BlueZMediaItemProps::objectPath),
      glz::field("player", &BlueZMediaItemProps::player),
      glz::field("name", &BlueZMediaItemProps::name),
      glz::field("type", &BlueZMediaItemProps::type),
      glz::field("folderType", &BlueZMediaItemProps::folderType),
      glz::field("playable", &BlueZMediaItemProps::playable),
      glz::field("metadata", &BlueZMediaItemProps::metadata));
};

struct BlueZMediaFolderItems {
  std::string objectPath;
  std::vector<BlueZMediaItemProps> items;
};
template <> struct glz::meta<BlueZMediaFolderItems> {
  static constexpr auto fields = std::make_tuple(
      glz::field("objectPath", &BlueZMediaFolderItems::objectPath),
      glz::field("items", &BlueZMediaFolderItems::items));
};

// ── Method results/errors ──────────────────────────────────────────────────

struct BlueZMediaAcquireResult {
  std::string transportPath;
  uint64_t fd{};
  uint16_t readMtu{};
  uint16_t writeMtu{};
};
template <> struct glz::meta<BlueZMediaAcquireResult> {
  static constexpr auto fields = std::make_tuple(
      glz::field("transportPath", &BlueZMediaAcquireResult::transportPath),
      glz::field("fd", &BlueZMediaAcquireResult::fd),
      glz::field("readMtu", &BlueZMediaAcquireResult::readMtu),
      glz::field("writeMtu", &BlueZMediaAcquireResult::writeMtu));
};

struct BlueZMediaError {
  std::string objectPath;
  std::string name;
  std::string message;
};
template <> struct glz::meta<BlueZMediaError> {
  static constexpr auto fields =
      std::make_tuple(glz::field("objectPath", &BlueZMediaError::objectPath),
                      glz::field("name", &BlueZMediaError::name),
                      glz::field("message", &BlueZMediaError::message));
};

struct BlueZMediaManagedObjects {
  std::vector<std::string> media;
  std::vector<std::string> players;
  std::vector<std::string> controls;
  std::vector<std::string> transports;
  std::vector<std::string> folders;
  std::vector<std::string> items;
};
template <> struct glz::meta<BlueZMediaManagedObjects> {
  static constexpr auto fields = std::make_tuple(
      glz::field("media", &BlueZMediaManagedObjects::media),
      glz::field("players", &BlueZMediaManagedObjects::players),
      glz::field("controls", &BlueZMediaManagedObjects::controls),
      glz::field("transports", &BlueZMediaManagedObjects::transports),
      glz::field("folders", &BlueZMediaManagedObjects::folders),
      glz::field("items", &BlueZMediaManagedObjects::items));
};

struct BlueZMediaObjectRemoved {
  std::string objectPath;
  std::string interfaceName;
};
template <> struct glz::meta<BlueZMediaObjectRemoved> {
  static constexpr auto fields = std::make_tuple(
      glz::field("objectPath", &BlueZMediaObjectRemoved::objectPath),
      glz::field("interfaceName", &BlueZMediaObjectRemoved::interfaceName));
};
