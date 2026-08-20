// test_media_types.cpp — glaze roundtrip tests for BlueZ Media wire structs.

#include "bluez_media_native.h"
#include "bluez_media_types.h"
#include "media_control_proxy.h"
#include "media_player_proxy.h"
#include "media_transport_proxy.h"
#include "media_utils.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {

// ── Existing roundtrip tests ─────────────────────────────────────────────────

void test_media_property_roundtrip() {
  BlueZMediaProperty orig;
  orig.key = "Title";
  orig.value = "Blue Train";

  auto buf = glz::encode(orig);
  BlueZMediaProperty decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.key == orig.key);
  assert(decoded.value == orig.value);
}

void test_media_player_props_roundtrip() {
  BlueZMediaPlayerProps orig;
  orig.objectPath = "/org/bluez/hci0/dev_AA/player0";
  orig.repeat = "off";
  orig.shuffle = "alltracks";
  orig.status = "playing";
  orig.position = 42000;
  orig.track = {{"Title", "Blue Train"}, {"Artist", "John Coltrane"}};
  orig.device = "/org/bluez/hci0/dev_AA";
  orig.name = "Media Player";
  orig.type = "audio";
  orig.subtype = "player";
  orig.browsable = true;
  orig.searchable = false;
  orig.playlist = "/org/bluez/hci0/dev_AA/player0/playlist";
  orig.obexPort = 0x1001;

  auto buf = glz::encode(orig);
  BlueZMediaPlayerProps decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.objectPath == orig.objectPath);
  assert(decoded.repeat == orig.repeat);
  assert(decoded.shuffle == orig.shuffle);
  assert(decoded.status == orig.status);
  assert(decoded.position == orig.position);
  assert(decoded.track.size() == 2u);
  assert(decoded.track[0].key == "Title");
  assert(decoded.track[0].value == "Blue Train");
  assert(decoded.track[1].key == "Artist");
  assert(decoded.track[1].value == "John Coltrane");
  assert(decoded.device == orig.device);
  assert(decoded.name == orig.name);
  assert(decoded.type == orig.type);
  assert(decoded.subtype == orig.subtype);
  assert(decoded.browsable == orig.browsable);
  assert(decoded.searchable == orig.searchable);
  assert(decoded.playlist == orig.playlist);
  assert(decoded.obexPort == orig.obexPort);
}

void test_media_control_props_roundtrip() {
  BlueZMediaControlProps orig;
  orig.objectPath = "/org/bluez/hci0/dev_AA";
  orig.connected = true;
  orig.player = "/org/bluez/hci0/dev_AA/player0";

  auto buf = glz::encode(orig);
  BlueZMediaControlProps decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.objectPath == orig.objectPath);
  assert(decoded.connected == orig.connected);
  assert(decoded.player == orig.player);
}

void test_media_transport_props_roundtrip() {
  BlueZMediaTransportProps orig;
  orig.objectPath = "/org/bluez/hci0/dev_AA/fd0";
  orig.device = "/org/bluez/hci0/dev_AA";
  orig.uuid = "0000110b-0000-1000-8000-00805f9b34fb";
  orig.codec = 0x00;
  orig.configuration = {0x21, 0x15};
  orig.state = "active";
  orig.delay = 120;
  orig.volume = 96;
  orig.endpoint = "/bluez_media/endpoint/a2dp_sink";

  auto buf = glz::encode(orig);
  BlueZMediaTransportProps decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.objectPath == orig.objectPath);
  assert(decoded.device == orig.device);
  assert(decoded.uuid == orig.uuid);
  assert(decoded.codec == orig.codec);
  assert(decoded.configuration == orig.configuration);
  assert(decoded.state == orig.state);
  assert(decoded.delay == orig.delay);
  assert(decoded.volume == orig.volume);
  assert(decoded.endpoint == orig.endpoint);
}

void test_media_item_props_roundtrip() {
  BlueZMediaItemProps orig;
  orig.objectPath = "/org/bluez/hci0/dev_AA/player0/item0";
  orig.player = "/org/bluez/hci0/dev_AA/player0";
  orig.name = "Blue Train";
  orig.type = "audio";
  orig.folderType = "album";
  orig.playable = true;
  orig.metadata = {{"Album", "Blue Train"}, {"Genre", "Jazz"}};

  auto buf = glz::encode(orig);
  BlueZMediaItemProps decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.objectPath == orig.objectPath);
  assert(decoded.player == orig.player);
  assert(decoded.name == orig.name);
  assert(decoded.type == orig.type);
  assert(decoded.folderType == orig.folderType);
  assert(decoded.playable == orig.playable);
  assert(decoded.metadata.size() == 2u);
  assert(decoded.metadata[0].key == "Album");
  assert(decoded.metadata[0].value == "Blue Train");
}

void test_media_folder_items_roundtrip() {
  BlueZMediaFolderItems orig;
  orig.objectPath = "/org/bluez/hci0/dev_AA/player0";
  orig.items = {
      BlueZMediaItemProps{.objectPath = "/org/bluez/hci0/dev_AA/player0/item0",
                          .player = "/org/bluez/hci0/dev_AA/player0",
                          .name = "Blue Train",
                          .type = "audio",
                          .folderType = "",
                          .playable = true,
                          .metadata = {{"Title", "Blue Train"}}},
      BlueZMediaItemProps{.objectPath =
                              "/org/bluez/hci0/dev_AA/player0/folder0",
                          .player = "/org/bluez/hci0/dev_AA/player0",
                          .name = "Albums",
                          .type = "folder",
                          .folderType = "album",
                          .playable = false,
                          .metadata = {}}};

  auto buf = glz::encode(orig);
  BlueZMediaFolderItems decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.objectPath == orig.objectPath);
  assert(decoded.items.size() == 2u);
  assert(decoded.items[0].objectPath == orig.items[0].objectPath);
  assert(decoded.items[0].name == "Blue Train");
  assert(decoded.items[0].playable);
  assert(decoded.items[0].metadata.size() == 1u);
  assert(decoded.items[1].type == "folder");
  assert(decoded.items[1].folderType == "album");
}

void test_method_result_roundtrips() {
  BlueZMediaAcquireResult acquire;
  acquire.transportPath = "/org/bluez/hci0/dev_AA/fd0";
  acquire.fd = -1;
  acquire.readMtu = 672;
  acquire.writeMtu = 672;

  auto acquire_buf = glz::encode(acquire);
  const auto fd_offset = sizeof(uint32_t) + acquire.transportPath.size();
  assert(acquire_buf.size() == fd_offset + sizeof(int32_t) +
                                   (2 * sizeof(uint16_t)));
  assert(acquire_buf[fd_offset] == 0xff);
  assert(acquire_buf[fd_offset + 1] == 0xff);
  assert(acquire_buf[fd_offset + 2] == 0xff);
  assert(acquire_buf[fd_offset + 3] == 0xff);
  BlueZMediaAcquireResult decoded_acquire;
  auto acquire_end = glz::decode(acquire_buf.data(), 0, decoded_acquire);

  assert(acquire_end == acquire_buf.size());
  assert(decoded_acquire.transportPath == acquire.transportPath);
  assert(decoded_acquire.fd == acquire.fd);
  assert(decoded_acquire.readMtu == acquire.readMtu);
  assert(decoded_acquire.writeMtu == acquire.writeMtu);
}

void test_player_properties_from_object_manager_payload() {
  const std::map<std::string, sdbus::Variant> track{
      {"Title", sdbus::Variant{std::string{"Blue Train"}}},
      {"ImgHandle", sdbus::Variant{std::string{"0000001"}}},
      {"Duration", sdbus::Variant{uint32_t{643000}}}};
  const std::map<std::string, sdbus::Variant> properties{
      {"Status", sdbus::Variant{std::string{"playing"}}},
      {"Position", sdbus::Variant{uint32_t{42000}}},
      {"Track", sdbus::Variant{track}},
      {"Browsable", sdbus::Variant{true}},
      // A malformed property must fall back instead of throwing in a signal
      // callback.
      {"Searchable", sdbus::Variant{std::string{"not-a-bool"}}}};

  const auto buffer = MediaPlayerProxy::encode_properties(
      "/org/bluez/hci0/dev_AA/player0", properties);
  BlueZMediaPlayerProps decoded;
  const auto end = glz::decode(buffer.data(), 0, decoded);

  assert(end == buffer.size());
  assert(decoded.status == "playing");
  assert(decoded.position == 42000u);
  assert(decoded.browsable);
  assert(!decoded.searchable);
  assert(decoded.track.size() == 3u);
  const auto image_handle = std::ranges::find_if(
      decoded.track,
      [](const auto& property) { return property.key == "ImgHandle"; });
  assert(image_handle != decoded.track.end());
  assert(image_handle->value == "0000001");
}

void test_cover_art_validates_arguments() {
  assert(bluez_media_player_get_cover_art(nullptr, "/player", "/tmp/art", 1) ==
         BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT);
}

void test_native_buffer_cleanup() {
  BluezMediaBuffer buffer{
      .data = static_cast<uint8_t*>(std::malloc(8)),
      .length = 8,
  };
  assert(buffer.data != nullptr);
  bluez_media_buffer_free(&buffer);
  assert(buffer.data == nullptr);
  assert(buffer.length == 0);
  bluez_media_buffer_free(&buffer);
  bluez_media_buffer_free(nullptr);
}

void test_object_manager_roundtrips() {
  BlueZMediaManagedObjects objects;
  objects.media = {"/org/bluez/hci0"};
  objects.players = {"/org/bluez/hci0/dev_AA/player0"};
  objects.controls = {"/org/bluez/hci0/dev_AA"};
  objects.transports = {"/org/bluez/hci0/dev_AA/sep1/fd0",
                        "/org/bluez/hci0/dev_BB/sep2/fd0"};
  objects.folders = {"/org/bluez/hci0/dev_AA/player0"};
  objects.items = {"/org/bluez/hci0/dev_AA/player0/item0"};

  auto objects_buf = glz::encode(objects);
  BlueZMediaManagedObjects decoded_objects;
  auto objects_end = glz::decode(objects_buf.data(), 0, decoded_objects);

  assert(objects_end == objects_buf.size());
  assert(decoded_objects.media == objects.media);
  assert(decoded_objects.players == objects.players);
  assert(decoded_objects.controls == objects.controls);
  assert(decoded_objects.transports == objects.transports);
  assert(decoded_objects.folders == objects.folders);
  assert(decoded_objects.items == objects.items);

  BlueZMediaObjectRemoved removed;
  removed.objectPath = "/org/bluez/hci0/dev_AA/sep1/fd0";
  removed.interfaceName = "org.bluez.MediaTransport1";

  auto removed_buf = glz::encode(removed);
  BlueZMediaObjectRemoved decoded_removed;
  auto removed_end = glz::decode(removed_buf.data(), 0, decoded_removed);

  assert(removed_end == removed_buf.size());
  assert(decoded_removed.objectPath == removed.objectPath);
  assert(decoded_removed.interfaceName == removed.interfaceName);
}

// ── New corner-case tests ────────────────────────────────────────────────────

// Empty / all-default structs ─────────────────────────────────────────────────

void test_media_player_props_all_defaults_roundtrip() {
  // Exercises the zero/default code paths (empty strings, 0 position,
  // false booleans, empty vectors).
  BlueZMediaPlayerProps orig;
  orig.objectPath = "/player";

  auto buf = glz::encode(orig);
  BlueZMediaPlayerProps decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.objectPath == "/player");
  assert(decoded.status.empty());
  assert(decoded.position == 0u);
  assert(decoded.track.empty());
  assert(!decoded.browsable);
  assert(!decoded.searchable);
  assert(decoded.obexPort == 0u);
}

void test_media_folder_props_roundtrip() {
  // BlueZMediaFolderProps was missing from the original suite.
  BlueZMediaFolderProps orig;
  orig.objectPath = "/org/bluez/hci0/dev_AA/player0/folder0";
  orig.numberOfItems = 42;
  orig.name = "Jazz";

  auto buf = glz::encode(orig);
  BlueZMediaFolderProps decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.objectPath == orig.objectPath);
  assert(decoded.numberOfItems == 42u);
  assert(decoded.name == "Jazz");
}

void test_media_folder_props_zero_items_roundtrip() {
  BlueZMediaFolderProps orig;
  orig.objectPath = "/folder";
  orig.numberOfItems = 0;
  orig.name = "";

  auto buf = glz::encode(orig);
  BlueZMediaFolderProps decoded;
  glz::decode(buf.data(), 0, decoded);

  assert(decoded.numberOfItems == 0u);
  assert(decoded.name.empty());
}

void test_media_folder_props_max_items_roundtrip() {
  // Verify uint32_t max survives the wire.
  BlueZMediaFolderProps orig;
  orig.objectPath = "/folder";
  orig.numberOfItems = std::numeric_limits<uint32_t>::max();
  orig.name = "Huge";

  auto buf = glz::encode(orig);
  BlueZMediaFolderProps decoded;
  glz::decode(buf.data(), 0, decoded);

  assert(decoded.numberOfItems == std::numeric_limits<uint32_t>::max());
}

void test_media_error_roundtrip() {
  // BlueZMediaError was not tested independently in the original suite.
  BlueZMediaError orig;
  orig.objectPath = "/org/bluez/hci0/dev_AA/player0";
  orig.name = "org.bluez.Error.Failed";
  orig.message = "Something went wrong";

  auto buf = glz::encode(orig);
  BlueZMediaError decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.objectPath == orig.objectPath);
  assert(decoded.name == orig.name);
  assert(decoded.message == orig.message);
}

void test_media_error_empty_fields_roundtrip() {
  // Empty strings in all three fields (e.g. connection failure with no path).
  BlueZMediaError orig;

  auto buf = glz::encode(orig);
  BlueZMediaError decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.objectPath.empty());
  assert(decoded.name.empty());
  assert(decoded.message.empty());
}

// Boundary values ─────────────────────────────────────────────────────────────

void test_acquire_result_zero_fd() {
  // fd=0 is technically valid (stdin is fd 0). Must survive the wire.
  BlueZMediaAcquireResult orig;
  orig.transportPath = "/transport";
  orig.fd = 0;
  orig.readMtu = 672;
  orig.writeMtu = 672;

  auto buf = glz::encode(orig);
  BlueZMediaAcquireResult decoded;
  glz::decode(buf.data(), 0, decoded);

  assert(decoded.fd == 0);
}

void test_acquire_result_max_fd() {
  // INT32_MAX simulates a high file-descriptor number.
  BlueZMediaAcquireResult orig;
  orig.transportPath = "/transport";
  orig.fd = std::numeric_limits<int32_t>::max();
  orig.readMtu = 0xFFFF;
  orig.writeMtu = 0xFFFF;

  auto buf = glz::encode(orig);
  BlueZMediaAcquireResult decoded;
  glz::decode(buf.data(), 0, decoded);

  assert(decoded.fd == std::numeric_limits<int32_t>::max());
  assert(decoded.readMtu == 0xFFFFu);
  assert(decoded.writeMtu == 0xFFFFu);
}

void test_transport_codec_max() {
  // codec field is uint8_t; 0xFF must survive.
  BlueZMediaTransportProps orig;
  orig.objectPath = "/transport";
  orig.codec = 0xFF;
  orig.delay = 0xFFFF;
  orig.volume = 127;  // BlueZ caps at 127

  auto buf = glz::encode(orig);
  BlueZMediaTransportProps decoded;
  glz::decode(buf.data(), 0, decoded);

  assert(decoded.codec == 0xFFu);
  assert(decoded.delay == 0xFFFFu);
  assert(decoded.volume == 127u);
}

void test_transport_empty_configuration() {
  BlueZMediaTransportProps orig;
  orig.objectPath = "/transport";
  // configuration left as empty vector

  auto buf = glz::encode(orig);
  BlueZMediaTransportProps decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.configuration.empty());
}

// encode_properties with no / partial D-Bus properties ───────────────────────

void test_player_encode_properties_empty_map() {
  // All properties missing → all fields fall back to their zero/empty default.
  const std::map<std::string, sdbus::Variant> empty;
  const auto buf = MediaPlayerProxy::encode_properties("/player", empty);

  BlueZMediaPlayerProps decoded;
  const auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.objectPath == "/player");
  assert(decoded.status.empty());
  assert(decoded.position == 0u);
  assert(decoded.track.empty());
  assert(!decoded.browsable);
  assert(decoded.obexPort == 0u);
}

void test_control_encode_properties_empty_map() {
  const std::map<std::string, sdbus::Variant> empty;
  const auto buf = MediaControlProxy::encode_properties("/control", empty);

  BlueZMediaControlProps decoded;
  const auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.objectPath == "/control");
  assert(!decoded.connected);
  assert(decoded.player.empty());
}

void test_transport_encode_properties_empty_map() {
  const std::map<std::string, sdbus::Variant> empty;
  const auto buf =
      MediaTransportProxy::encode_properties("/transport", empty);

  BlueZMediaTransportProps decoded;
  const auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.objectPath == "/transport");
  assert(decoded.state.empty());
  assert(decoded.configuration.empty());
  assert(decoded.codec == 0u);
}

void test_player_encode_properties_type_mismatch_falls_back() {
  // Property exists but with wrong D-Bus type → media_property returns default.
  const std::map<std::string, sdbus::Variant> properties{
      {"Position",
       sdbus::Variant{std::string{"not-a-uint32"}}},  // wrong type
      {"Browsable",
       sdbus::Variant{uint32_t{1}}},  // wrong type – should be bool
      {"ObexPort",
       sdbus::Variant{std::string{"4097"}}}};  // wrong type

  const auto buf =
      MediaPlayerProxy::encode_properties("/player", properties);
  BlueZMediaPlayerProps decoded;
  glz::decode(buf.data(), 0, decoded);

  // All mismatched properties must produce safe defaults.
  assert(decoded.position == 0u);
  assert(!decoded.browsable);
  assert(decoded.obexPort == 0u);
}

// bluez_media_close_fd ────────────────────────────────────────────────────────

void test_close_fd_negative_returns_invalid_argument() {
  // Passing a negative fd must not crash and must return INVALID_ARGUMENT.
  assert(bluez_media_close_fd(-1) == BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT);
  assert(bluez_media_close_fd(-100) == BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT);
  assert(bluez_media_close_fd(std::numeric_limits<int32_t>::min()) ==
         BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT);
}

// Empty object manager lists ──────────────────────────────────────────────────

void test_object_manager_empty_lists_roundtrip() {
  BlueZMediaManagedObjects orig;  // all vectors are empty by default

  auto buf = glz::encode(orig);
  BlueZMediaManagedObjects decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.media.empty());
  assert(decoded.players.empty());
  assert(decoded.controls.empty());
  assert(decoded.transports.empty());
  assert(decoded.folders.empty());
  assert(decoded.items.empty());
}

// UTF-8 multibyte strings ─────────────────────────────────────────────────────

void test_utf8_multibyte_string_roundtrip() {
  // "蓝色火车" is "Blue Train" in Chinese — 4 chars, 12 UTF-8 bytes.
  // Verifies that the length field stores byte count, not char count.
  BlueZMediaProperty orig;
  orig.key = "Title";
  orig.value = "\xe8\x93\x9d\xe8\x89\xb2\xe7\x81\xab\xe8\xbd\xa6";

  auto buf = glz::encode(orig);
  BlueZMediaProperty decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.value == orig.value);
}

// track_to_properties / variant_to_string ─────────────────────────────────────

void test_track_to_properties_numeric_and_bool_variants() {
  // variant_to_string must handle non-string D-Bus variants without crashing.
  const std::map<std::string, sdbus::Variant> track{
      {"Duration", sdbus::Variant{uint32_t{643000}}},  // uint32_t path
      {"Genre", sdbus::Variant{std::string{"Jazz"}}},   // string path
      {"TrackNumber", sdbus::Variant{uint16_t{1}}},     // uint16_t path
      {"Loved", sdbus::Variant{true}},                   // bool path
  };

  const auto props = track_to_properties(track);

  // Result must contain exactly 4 entries.
  assert(props.size() == 4u);

  // Verify specific conversions.
  for (const auto& p : props) {
    if (p.key == "Duration") {
      assert(p.value == "643000");
    } else if (p.key == "Genre") {
      assert(p.value == "Jazz");
    } else if (p.key == "TrackNumber") {
      assert(p.value == "1");
    } else if (p.key == "Loved") {
      assert(p.value == "true");
    }
  }
}

void test_track_to_properties_empty_track() {
  const std::map<std::string, sdbus::Variant> empty;
  const auto props = track_to_properties(empty);
  assert(props.empty());
}

void test_track_to_properties_object_path_variant() {
  // sdbus::ObjectPath must be handled by variant_to_string.
  const std::map<std::string, sdbus::Variant> track{
      {"AlbumArtURL",
       sdbus::Variant{sdbus::ObjectPath{"/org/example/art"}}},
  };
  const auto props = track_to_properties(track);
  assert(props.size() == 1u);
  assert(props[0].value == "/org/example/art");
}

// cover_art additional validation ─────────────────────────────────────────────

void test_cover_art_rejects_zero_timeout() {
  // timeout_ms <= 0 must return INVALID_ARGUMENT.
  assert(bluez_media_player_get_cover_art(nullptr, "/p", "/abs/file", 0) ==
         BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT);
  assert(bluez_media_player_get_cover_art(nullptr, "/p", "/abs/file", -1) ==
         BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT);
}

void test_cover_art_rejects_null_player_path() {
  assert(bluez_media_player_get_cover_art(nullptr, nullptr, "/tmp/art", 1) ==
         BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT);
}

void test_cover_art_rejects_null_target_file() {
  assert(bluez_media_player_get_cover_art(nullptr, "/player", nullptr, 1) ==
         BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT);
}

}  // namespace

int main() {
  // ── Original tests ────────────────────────────────────────────────────────
  test_media_property_roundtrip();
  test_media_player_props_roundtrip();
  test_media_control_props_roundtrip();
  test_media_transport_props_roundtrip();
  test_media_item_props_roundtrip();
  test_media_folder_items_roundtrip();
  test_method_result_roundtrips();
  test_player_properties_from_object_manager_payload();
  test_cover_art_validates_arguments();
  test_native_buffer_cleanup();
  test_object_manager_roundtrips();

  // ── New corner-case tests ─────────────────────────────────────────────────
  test_media_player_props_all_defaults_roundtrip();
  test_media_folder_props_roundtrip();
  test_media_folder_props_zero_items_roundtrip();
  test_media_folder_props_max_items_roundtrip();
  test_media_error_roundtrip();
  test_media_error_empty_fields_roundtrip();
  test_acquire_result_zero_fd();
  test_acquire_result_max_fd();
  test_transport_codec_max();
  test_transport_empty_configuration();
  test_player_encode_properties_empty_map();
  test_control_encode_properties_empty_map();
  test_transport_encode_properties_empty_map();
  test_player_encode_properties_type_mismatch_falls_back();
  test_close_fd_negative_returns_invalid_argument();
  test_object_manager_empty_lists_roundtrip();
  test_utf8_multibyte_string_roundtrip();
  test_track_to_properties_numeric_and_bool_variants();
  test_track_to_properties_empty_track();
  test_track_to_properties_object_path_variant();
  test_cover_art_rejects_zero_timeout();
  test_cover_art_rejects_null_player_path();
  test_cover_art_rejects_null_target_file();

  return 0;
}
