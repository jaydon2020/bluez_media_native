import 'dart:convert';
import 'dart:typed_data';

import 'package:bluez_media_native/src/ffi/codec.dart';
import 'package:bluez_media_native/src/ffi/types.dart';
import 'package:flutter_test/flutter_test.dart';

/// Helper: encode a string as length-prefixed UTF-8 (matches glaze_meta.h).
void _writeString(BytesBuilder b, String s) {
  final bytes = Uint8List.fromList(utf8.encode(s));
  final len = ByteData(4)..setUint32(0, bytes.length, Endian.little);
  b.add(len.buffer.asUint8List());
  b.add(bytes);
}

void _writeBool(BytesBuilder b, bool v) {
  b.addByte(v ? 1 : 0);
}

void _writeUint8(BytesBuilder b, int v) {
  b.addByte(v);
}

void _writeUint16(BytesBuilder b, int v) {
  final d = ByteData(2)..setUint16(0, v, Endian.little);
  b.add(d.buffer.asUint8List());
}

void _writeUint32(BytesBuilder b, int v) {
  final d = ByteData(4)..setUint32(0, v, Endian.little);
  b.add(d.buffer.asUint8List());
}

void _writeUint64(BytesBuilder b, int v) {
  final d = ByteData(8)..setUint64(0, v, Endian.little);
  b.add(d.buffer.asUint8List());
}

void _writeByteList(BytesBuilder b, List<int> bytes) {
  _writeUint32(b, bytes.length);
  b.add(bytes);
}

void _writeMediaProperties(BytesBuilder b, Map<String, String> properties) {
  _writeUint32(b, properties.length);
  for (final entry in properties.entries) {
    _writeString(b, entry.key);
    _writeString(b, entry.value);
  }
}

void main() {
  group('GlazeCodec', () {
    test('decodes BlueZMediaProperty', () {
      final b = BytesBuilder();
      _writeString(b, 'Title');
      _writeString(b, 'Blue Train');

      final data = Uint8List.fromList(b.toBytes());
      final prop = GlazeCodec.decode<BlueZMediaProperty>(data, 0);

      expect(prop.key, 'Title');
      expect(prop.value, 'Blue Train');
    });

    test('decodes BlueZMediaPlayerProps', () {
      final b = BytesBuilder();
      _writeString(b, '/org/bluez/hci0/dev_AA/player0');
      _writeUint32(b, 0xFFFFFFFF);
      _writeString(b, 'off'); // equalizer
      _writeString(b, 'singletrack'); // repeat
      _writeString(b, 'alltracks'); // shuffle
      _writeString(b, 'alltracks'); // scan
      _writeString(b, 'playing'); // status
      _writeUint32(b, 42000); // position
      _writeMediaProperties(b, {'Title': 'Blue Train', 'Artist': 'Coltrane'});
      _writeString(b, '/org/bluez/hci0/dev_AA');
      _writeString(b, 'Media Player');
      _writeString(b, 'audio');
      _writeString(b, 'player');
      _writeBool(b, true);
      _writeBool(b, false);
      _writeString(b, '/org/bluez/hci0/dev_AA/player0/playlist');

      final data = Uint8List.fromList(b.toBytes());
      final props = GlazeCodec.decode<BlueZMediaPlayerProps>(data, 0);

      expect(props.objectPath, '/org/bluez/hci0/dev_AA/player0');
      expect(props.changedMask, 0xFFFFFFFF);
      expect(props.repeat, 'singletrack');
      expect(props.shuffle, 'alltracks');
      expect(props.status, 'playing');
      expect(props.position, 42000);
      expect(props.track.length, 2);
      expect(props.track[0].key, 'Title');
      expect(props.track[0].value, 'Blue Train');
      expect(props.track[1].key, 'Artist');
      expect(props.track[1].value, 'Coltrane');
      expect(props.device, '/org/bluez/hci0/dev_AA');
      expect(props.browsable, true);
      expect(props.searchable, false);
      expect(props.playlist, '/org/bluez/hci0/dev_AA/player0/playlist');
    });

    test('decodes BlueZMediaControlProps', () {
      final b = BytesBuilder();
      _writeString(b, '/org/bluez/hci0/dev_AA');
      _writeUint32(b, 0x3);
      _writeBool(b, true);
      _writeString(b, '/org/bluez/hci0/dev_AA/player0');

      final data = Uint8List.fromList(b.toBytes());
      final props = GlazeCodec.decode<BlueZMediaControlProps>(data, 0);

      expect(props.objectPath, '/org/bluez/hci0/dev_AA');
      expect(props.changedMask, 0x3);
      expect(props.connected, true);
      expect(props.player, '/org/bluez/hci0/dev_AA/player0');
    });

    test('decodes BlueZMediaEndpointProps', () {
      final b = BytesBuilder();
      _writeString(b, '/bluez_media/endpoint/a2dp_sink');
      _writeString(b, '0000110b-0000-1000-8000-00805f9b34fb');
      _writeUint8(b, 0x00);
      _writeByteList(b, [0x3f, 0xff, 0x02, 0x35]);
      _writeString(b, '/org/bluez/hci0/dev_AA');
      _writeBool(b, true);

      final data = Uint8List.fromList(b.toBytes());
      final props = GlazeCodec.decode<BlueZMediaEndpointProps>(data, 0);

      expect(props.objectPath, '/bluez_media/endpoint/a2dp_sink');
      expect(props.uuid, '0000110b-0000-1000-8000-00805f9b34fb');
      expect(props.codec, 0);
      expect(props.capabilities, [0x3f, 0xff, 0x02, 0x35]);
      expect(props.device, '/org/bluez/hci0/dev_AA');
      expect(props.delayReporting, true);
    });

    test('decodes BlueZMediaTransportProps', () {
      final b = BytesBuilder();
      _writeString(b, '/org/bluez/hci0/dev_AA/fd0');
      _writeString(b, '/org/bluez/hci0/dev_AA');
      _writeString(b, '0000110b-0000-1000-8000-00805f9b34fb');
      _writeUint8(b, 0x00);
      _writeByteList(b, [0x21, 0x15]);
      _writeString(b, 'active');
      _writeUint16(b, 120);
      _writeUint16(b, 96);
      _writeString(b, '/bluez_media/endpoint/a2dp_sink');

      final data = Uint8List.fromList(b.toBytes());
      final props = GlazeCodec.decode<BlueZMediaTransportProps>(data, 0);

      expect(props.objectPath, '/org/bluez/hci0/dev_AA/fd0');
      expect(props.device, '/org/bluez/hci0/dev_AA');
      expect(props.codec, 0);
      expect(props.configuration, [0x21, 0x15]);
      expect(props.state, 'active');
      expect(props.delay, 120);
      expect(props.volume, 96);
      expect(props.endpoint, '/bluez_media/endpoint/a2dp_sink');
    });

    test('decodes BlueZMediaFolderProps', () {
      final b = BytesBuilder();
      _writeString(b, '/org/bluez/hci0/dev_AA/player0/folder0');
      _writeUint32(b, 23);
      _writeString(b, 'Albums');

      final data = Uint8List.fromList(b.toBytes());
      final props = GlazeCodec.decode<BlueZMediaFolderProps>(data, 0);

      expect(props.objectPath, '/org/bluez/hci0/dev_AA/player0/folder0');
      expect(props.numberOfItems, 23);
      expect(props.name, 'Albums');
    });

    test('decodes BlueZMediaItemProps', () {
      final b = BytesBuilder();
      _writeString(b, '/org/bluez/hci0/dev_AA/player0/item0');
      _writeString(b, '/org/bluez/hci0/dev_AA/player0');
      _writeString(b, 'Blue Train');
      _writeString(b, 'audio');
      _writeString(b, 'album');
      _writeBool(b, true);
      _writeMediaProperties(b, {'Album': 'Blue Train', 'Genre': 'Jazz'});

      final data = Uint8List.fromList(b.toBytes());
      final props = GlazeCodec.decode<BlueZMediaItemProps>(data, 0);

      expect(props.objectPath, '/org/bluez/hci0/dev_AA/player0/item0');
      expect(props.player, '/org/bluez/hci0/dev_AA/player0');
      expect(props.name, 'Blue Train');
      expect(props.type, 'audio');
      expect(props.folderType, 'album');
      expect(props.playable, true);
      expect(props.metadata.length, 2);
      expect(props.metadata[0].key, 'Album');
      expect(props.metadata[1].value, 'Jazz');
    });

    test('decodes BlueZMediaAcquireResult', () {
      final b = BytesBuilder();
      _writeString(b, '/org/bluez/hci0/dev_AA/fd0');
      _writeUint64(b, 42);
      _writeUint16(b, 672);
      _writeUint16(b, 672);

      final data = Uint8List.fromList(b.toBytes());
      final result = GlazeCodec.decode<BlueZMediaAcquireResult>(data, 0);

      expect(result.transportPath, '/org/bluez/hci0/dev_AA/fd0');
      expect(result.fd, 42);
      expect(result.readMtu, 672);
      expect(result.writeMtu, 672);
    });

    test('decodes BlueZMediaError with offset', () {
      final b = BytesBuilder();
      b.addByte(0x20); // discriminator
      _writeString(b, '/org/bluez/hci0/dev_AA/fd0');
      _writeString(b, 'org.bluez.Error.NotAvailable');
      _writeString(b, 'Transport is not available');

      final data = Uint8List.fromList(b.toBytes());
      final err = GlazeCodec.decode<BlueZMediaError>(data, 1);

      expect(err.objectPath, '/org/bluez/hci0/dev_AA/fd0');
      expect(err.name, 'org.bluez.Error.NotAvailable');
      expect(err.message, 'Transport is not available');
    });

    test('throws on unknown type', () {
      final data = Uint8List(0);
      expect(() => GlazeCodec.decode<int>(data, 0), throwsArgumentError);
    });

    test('throws on read overrun', () {
      final data = Uint8List.fromList([0x01, 0x02]);
      expect(
        () => GlazeCodec.decode<BlueZMediaFolderProps>(data, 0),
        throwsRangeError,
      );
    });
  });
}
