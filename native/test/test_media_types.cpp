// test_media_types.cpp — glaze roundtrip tests for BlueZ Media wire structs.

#include "bluez_media_types.h"

#include <cassert>
#include <vector>

namespace {

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
  orig.changedMask = 0xFFFFFFFFu;
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

  auto buf = glz::encode(orig);
  BlueZMediaPlayerProps decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.objectPath == orig.objectPath);
  assert(decoded.changedMask == orig.changedMask);
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
}

void test_media_control_props_roundtrip() {
  BlueZMediaControlProps orig;
  orig.objectPath = "/org/bluez/hci0/dev_AA";
  orig.changedMask = 0x3u;
  orig.connected = true;
  orig.player = "/org/bluez/hci0/dev_AA/player0";

  auto buf = glz::encode(orig);
  BlueZMediaControlProps decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.objectPath == orig.objectPath);
  assert(decoded.changedMask == orig.changedMask);
  assert(decoded.connected == orig.connected);
  assert(decoded.player == orig.player);
}

void test_media_endpoint_props_roundtrip() {
  BlueZMediaEndpointProps orig;
  orig.objectPath = "/bluez_media/endpoint/a2dp_sink";
  orig.uuid = "0000110b-0000-1000-8000-00805f9b34fb";
  orig.codec = 0x00;
  orig.capabilities = {0x3f, 0xff, 0x02, 0x35};
  orig.device = "/org/bluez/hci0/dev_AA";
  orig.delayReporting = true;

  auto buf = glz::encode(orig);
  BlueZMediaEndpointProps decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  assert(end == buf.size());
  assert(decoded.objectPath == orig.objectPath);
  assert(decoded.uuid == orig.uuid);
  assert(decoded.codec == orig.codec);
  assert(decoded.capabilities == orig.capabilities);
  assert(decoded.device == orig.device);
  assert(decoded.delayReporting == orig.delayReporting);
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

void test_method_result_roundtrips() {
  BlueZMediaAcquireResult acquire;
  acquire.transportPath = "/org/bluez/hci0/dev_AA/fd0";
  acquire.fd = 42;
  acquire.readMtu = 672;
  acquire.writeMtu = 672;

  auto acquire_buf = glz::encode(acquire);
  BlueZMediaAcquireResult decoded_acquire;
  auto acquire_end = glz::decode(acquire_buf.data(), 0, decoded_acquire);

  assert(acquire_end == acquire_buf.size());
  assert(decoded_acquire.transportPath == acquire.transportPath);
  assert(decoded_acquire.fd == acquire.fd);
  assert(decoded_acquire.readMtu == acquire.readMtu);
  assert(decoded_acquire.writeMtu == acquire.writeMtu);

  BlueZMediaError error;
  error.objectPath = "/org/bluez/hci0/dev_AA/fd0";
  error.name = "org.bluez.Error.NotAvailable";
  error.message = "Transport is not available";

  auto error_buf = glz::encode(error);
  BlueZMediaError decoded_error;
  auto error_end = glz::decode(error_buf.data(), 0, decoded_error);

  assert(error_end == error_buf.size());
  assert(decoded_error.objectPath == error.objectPath);
  assert(decoded_error.name == error.name);
  assert(decoded_error.message == error.message);
}

}  // namespace

int main() {
  test_media_property_roundtrip();
  test_media_player_props_roundtrip();
  test_media_control_props_roundtrip();
  test_media_endpoint_props_roundtrip();
  test_media_transport_props_roundtrip();
  test_media_item_props_roundtrip();
  test_method_result_roundtrips();
  return 0;
}
