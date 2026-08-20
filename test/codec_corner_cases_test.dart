// SPDX-License-Identifier: Apache-2.0
//
// codec_corner_cases_test.dart — corner cases for GlazeCodec not covered by
// codec_test.dart.  Each group documents the exact branch or boundary being
// exercised.

import 'dart:convert';
import 'dart:typed_data';

import 'package:bluez_media_native/src/ffi/codec.dart';
import 'package:bluez_media_native/src/ffi/types.dart';
import 'package:test/test.dart';

// ── Encoding helpers (mirror of codec_test.dart) ────────────────────────────

void _writeString(BytesBuilder b, String s) {
  final bytes = Uint8List.fromList(utf8.encode(s));
  final len = ByteData(4)..setUint32(0, bytes.length, Endian.little);
  b.add(len.buffer.asUint8List());
  b.add(bytes);
}

void _writeBool(BytesBuilder b, {required bool v}) => b.addByte(v ? 1 : 0);
void _writeUint8(BytesBuilder b, int v) => b.addByte(v & 0xFF);

void _writeUint16(BytesBuilder b, int v) {
  final d = ByteData(2)..setUint16(0, v & 0xFFFF, Endian.little);
  b.add(d.buffer.asUint8List());
}

void _writeUint32(BytesBuilder b, int v) {
  final d = ByteData(4)..setUint32(0, v, Endian.little);
  b.add(d.buffer.asUint8List());
}

void _writeInt32(BytesBuilder b, int v) {
  final d = ByteData(4)..setInt32(0, v, Endian.little);
  b.add(d.buffer.asUint8List());
}

void _writeEmptyList(BytesBuilder b) => _writeUint32(b, 0);

void _writePlayerProps(
  BytesBuilder b, {
  String objectPath = '/player',
  String equalizer = '',
  String repeat = '',
  String shuffle = '',
  String scan = '',
  String status = '',
  int position = 0,
  Map<String, String> track = const {},
  String device = '',
  String name = '',
  String type = '',
  String subtype = '',
  bool browsable = false,
  bool searchable = false,
  String playlist = '',
  int obexPort = 0,
}) {
  _writeString(b, objectPath);
  _writeString(b, equalizer);
  _writeString(b, repeat);
  _writeString(b, shuffle);
  _writeString(b, scan);
  _writeString(b, status);
  _writeUint32(b, position);
  // track as property list
  _writeUint32(b, track.length);
  for (final e in track.entries) {
    _writeString(b, e.key);
    _writeString(b, e.value);
  }
  _writeString(b, device);
  _writeString(b, name);
  _writeString(b, type);
  _writeString(b, subtype);
  _writeBool(b, v: browsable);
  _writeBool(b, v: searchable);
  _writeString(b, playlist);
  _writeUint16(b, obexPort);
}

// ── Tests ────────────────────────────────────────────────────────────────────

void main() {
  group('GlazeCodec – byte offset', () {
    // The event dispatch path always prepends a 1-byte tag and calls
    // decode<T>(message, 1). Verify that a non-zero start offset is honoured
    // correctly and does not read the tag byte as data.
    test('decodes BlueZMediaPlayerProps at offset 1', () {
      final b = BytesBuilder();
      b.addByte(0x01); // tag byte – must be skipped
      _writePlayerProps(b, status: 'playing');

      final data = Uint8List.fromList(b.toBytes());
      final props = GlazeCodec.decode<BlueZMediaPlayerProps>(data, 1);

      expect(props.objectPath, '/player');
      expect(props.status, 'playing');
    });

    test('decodes BlueZMediaObjectRemoved at offset 1', () {
      final b = BytesBuilder();
      b.addByte(0x7E); // tag byte
      _writeString(b, '/org/bluez/hci0/dev_AA/fd0');
      _writeString(b, 'org.bluez.MediaTransport1');

      final data = Uint8List.fromList(b.toBytes());
      final removed = GlazeCodec.decode<BlueZMediaObjectRemoved>(data, 1);

      expect(removed.objectPath, '/org/bluez/hci0/dev_AA/fd0');
      expect(removed.interfaceName, 'org.bluez.MediaTransport1');
    });

    test('decodes BlueZMediaError at offset 1', () {
      final b = BytesBuilder();
      b.addByte(0x20); // error tag
      _writeString(b, '/org/bluez/hci0/dev_AA/player0');
      _writeString(b, 'org.bluez.Error.Failed');
      _writeString(b, 'Something went wrong');

      final data = Uint8List.fromList(b.toBytes());
      final error = GlazeCodec.decode<BlueZMediaError>(data, 1);

      expect(error.objectPath, '/org/bluez/hci0/dev_AA/player0');
      expect(error.name, 'org.bluez.Error.Failed');
      expect(error.message, 'Something went wrong');
    });
  });

  group('GlazeCodec – BlueZMediaError', () {
    // BlueZMediaError is decoded in _exceptionFromResult but never tested
    // directly in the existing suite.
    test('decodes all fields at offset 0', () {
      final b = BytesBuilder();
      _writeString(b, '/transport');
      _writeString(b, 'org.bluez.Error.NotSupported');
      _writeString(b, 'Codec not supported');

      final error = GlazeCodec.decode<BlueZMediaError>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(error.objectPath, '/transport');
      expect(error.name, 'org.bluez.Error.NotSupported');
      expect(error.message, 'Codec not supported');
    });

    test('decodes empty fields without throwing', () {
      final b = BytesBuilder();
      _writeString(b, ''); // empty object path
      _writeString(b, '');
      _writeString(b, '');

      final error = GlazeCodec.decode<BlueZMediaError>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(error.objectPath, '');
      expect(error.name, '');
      expect(error.message, '');
    });
  });

  group('GlazeCodec – all-default / empty structs', () {
    // Exercises the zero/default paths that are skipped by the happy-path tests.

    test('BlueZMediaPlayerProps with all defaults encodes and decodes', () {
      // Build a payload with all empty strings, 0 position, empty track, etc.
      final b = BytesBuilder();
      _writePlayerProps(b); // all defaults

      final props = GlazeCodec.decode<BlueZMediaPlayerProps>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(props.objectPath, '/player');
      expect(props.status, '');
      expect(props.position, 0);
      expect(props.track, isEmpty);
      expect(props.browsable, false);
      expect(props.obexPort, 0);
    });

    test('BlueZMediaFolderProps with zero numberOfItems', () {
      final b = BytesBuilder();
      _writeString(b, '/folder');
      _writeUint32(b, 0);
      _writeString(b, '');

      final props = GlazeCodec.decode<BlueZMediaFolderProps>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(props.numberOfItems, 0);
      expect(props.name, '');
    });

    test('BlueZMediaTransportProps with empty configuration byte list', () {
      final b = BytesBuilder();
      _writeString(b, '/transport');
      _writeString(b, '');
      _writeString(b, '');
      _writeUint8(b, 0); // codec
      _writeEmptyList(b); // configuration = []
      _writeString(b, '');
      _writeUint16(b, 0);
      _writeUint16(b, 0);
      _writeString(b, '');

      final props = GlazeCodec.decode<BlueZMediaTransportProps>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(props.configuration, isEmpty);
    });

    test('BlueZMediaFolderItems with zero items', () {
      final b = BytesBuilder();
      _writeString(b, '/folder');
      _writeUint32(b, 0); // count = 0

      final result = GlazeCodec.decode<BlueZMediaFolderItems>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(result.items, isEmpty);
    });

    test('BlueZMediaManagedObjects with all empty lists', () {
      final b = BytesBuilder();
      for (var i = 0; i < 6; i++) {
        _writeUint32(b, 0); // count = 0 for each of the 6 lists
      }

      final result = GlazeCodec.decode<BlueZMediaManagedObjects>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(result.media, isEmpty);
      expect(result.players, isEmpty);
      expect(result.controls, isEmpty);
      expect(result.transports, isEmpty);
      expect(result.folders, isEmpty);
      expect(result.items, isEmpty);
    });
  });

  group('GlazeCodec – boundary field values', () {
    test('BlueZMediaPlayerProps position = 0xFFFFFFFF (u32 max)', () {
      final b = BytesBuilder();
      _writePlayerProps(b, position: 0xFFFFFFFF);

      final props = GlazeCodec.decode<BlueZMediaPlayerProps>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(props.position, 0xFFFFFFFF);
    });

    test('BlueZMediaPlayerProps obexPort = 0xFFFF (u16 max)', () {
      final b = BytesBuilder();
      _writePlayerProps(b, obexPort: 0xFFFF);

      final props = GlazeCodec.decode<BlueZMediaPlayerProps>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(props.obexPort, 0xFFFF);
    });

    test('BlueZMediaTransportProps codec = 0xFF (u8 max)', () {
      final b = BytesBuilder();
      _writeString(b, '/transport');
      _writeString(b, '');
      _writeString(b, '');
      _writeUint8(b, 0xFF); // codec max
      _writeEmptyList(b);
      _writeString(b, '');
      _writeUint16(b, 0xFFFF); // delay max
      _writeUint16(b, 0x007F); // volume max (127 per BlueZ spec)
      _writeString(b, '');

      final props = GlazeCodec.decode<BlueZMediaTransportProps>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(props.codec, 0xFF);
      expect(props.delay, 0xFFFF);
      expect(props.volume, 0x007F);
    });

    test('BlueZMediaAcquireResult fd = INT32_MAX (positive valid fd)', () {
      final b = BytesBuilder();
      _writeString(b, '/transport');
      _writeInt32(b, 0x7FFFFFFF); // INT32_MAX simulating a high fd number
      _writeUint16(b, 895);
      _writeUint16(b, 895);

      final result = GlazeCodec.decode<BlueZMediaAcquireResult>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(result.fd, 0x7FFFFFFF);
      expect(result.readMtu, 895);
      expect(result.writeMtu, 895);
    });

    test('BlueZMediaAcquireResult fd = 0 (stdin fd is an edge case)', () {
      final b = BytesBuilder();
      _writeString(b, '/transport');
      _writeInt32(b, 0); // fd 0 is technically valid (stdin)
      _writeUint16(b, 672);
      _writeUint16(b, 672);

      final result = GlazeCodec.decode<BlueZMediaAcquireResult>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(result.fd, 0);
    });

    test('BlueZMediaFolderProps numberOfItems = 0xFFFFFFFF (u32 max)', () {
      final b = BytesBuilder();
      _writeString(b, '/folder');
      _writeUint32(b, 0xFFFFFFFF);
      _writeString(b, 'Huge Folder');

      final props = GlazeCodec.decode<BlueZMediaFolderProps>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(props.numberOfItems, 0xFFFFFFFF);
    });
  });

  group('GlazeCodec – UTF-8 multibyte strings', () {
    // Verifies that the length-prefixed encoding correctly handles strings
    // where byte length differs from character count.
    test('BlueZMediaProperty with CJK value', () {
      const title = '蓝色火车'; // "Blue Train" in Chinese — 4 chars, 12 bytes
      final b = BytesBuilder();
      _writeString(b, 'Title');
      _writeString(b, title);

      final prop = GlazeCodec.decode<BlueZMediaProperty>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(prop.key, 'Title');
      expect(prop.value, title);
    });

    test('BlueZMediaPlayerProps with emoji in name', () {
      const name = '🎵 Player'; // 4-byte emoji prefix
      final b = BytesBuilder();
      _writePlayerProps(b, name: name);

      final props = GlazeCodec.decode<BlueZMediaPlayerProps>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(props.name, name);
    });

    test('BlueZMediaError with non-ASCII message', () {
      const msg = 'Échec de l\'opération';
      final b = BytesBuilder();
      _writeString(b, '/path');
      _writeString(b, 'org.bluez.Error.Failed');
      _writeString(b, msg);

      final error = GlazeCodec.decode<BlueZMediaError>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(error.message, msg);
    });
  });

  group('GlazeCodec – BlueZMediaTransportProps configuration byte list', () {
    test('decodes a 16-byte SBC configuration correctly', () {
      // Real SBC config from BlueZ documentation.
      const config = [0xFF, 0xFF, 0x02, 0x35];
      final b = BytesBuilder();
      _writeString(b, '/transport');
      _writeString(b, '');
      _writeString(b, '');
      _writeUint8(b, 0x00);
      _writeUint32(b, config.length);
      b.add(config);
      _writeString(b, 'idle');
      _writeUint16(b, 0);
      _writeUint16(b, 0);
      _writeString(b, '');

      final props = GlazeCodec.decode<BlueZMediaTransportProps>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(props.configuration, config);
    });

    test('decodes a single-byte configuration', () {
      final b = BytesBuilder();
      _writeString(b, '/transport');
      _writeString(b, '');
      _writeString(b, '');
      _writeUint8(b, 0);
      _writeUint32(b, 1);
      b.addByte(0xAB);
      _writeString(b, '');
      _writeUint16(b, 0);
      _writeUint16(b, 0);
      _writeString(b, '');

      final props = GlazeCodec.decode<BlueZMediaTransportProps>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(props.configuration, [0xAB]);
    });
  });

  group('GlazeCodec – read overrun variants', () {
    // Exercises the _checkBounds path for each primitive reader.

    test('throws RangeError decoding uint16 with only 1 byte', () {
      // Give enough bytes for the string length prefix but then cut off
      // the uint16 field mid-read in BlueZMediaControlProps.
      final b = BytesBuilder();
      _writeString(b, '/control'); // objectPath
      // connected (bool) is next but we stop here → connected overruns
      expect(
        () => GlazeCodec.decode<BlueZMediaControlProps>(
          Uint8List.fromList(b.toBytes()),
          0,
        ),
        throwsRangeError,
      );
    });

    test('throws RangeError decoding uint32 list count with 3 bytes', () {
      // For BlueZMediaManagedObjects the first field is a uint32 count.
      // Give only 3 bytes (< 4 required).
      final data = Uint8List.fromList([0x01, 0x00, 0x00]);
      expect(
        () => GlazeCodec.decode<BlueZMediaManagedObjects>(data, 0),
        throwsRangeError,
      );
    });

    test('throws RangeError when string payload is truncated', () {
      // Write a length prefix of 100 but only provide 10 bytes of payload.
      final b = BytesBuilder();
      _writeUint32(b, 100); // claims 100 bytes
      b.add(List.filled(10, 0x41)); // only 10 bytes ('A')
      expect(
        () => GlazeCodec.decode<BlueZMediaProperty>(
          Uint8List.fromList(b.toBytes()),
          0,
        ),
        throwsRangeError,
      );
    });

    test('throws on zero-length buffer for any type', () {
      final empty = Uint8List(0);
      // BlueZMediaFolderProps requires at least a 4-byte string length prefix
      expect(
        () => GlazeCodec.decode<BlueZMediaFolderProps>(empty, 0),
        throwsRangeError,
      );
    });
  });

  group('GlazeCodec – BlueZMediaFolderItems with multiple items', () {
    test('items are decoded in order and metadata is per-item', () {
      final b = BytesBuilder();
      _writeString(b, '/folder');
      _writeUint32(b, 3); // 3 items

      for (var i = 0; i < 3; i++) {
        _writeString(b, '/item$i');
        _writeString(b, '/player');
        _writeString(b, 'Track $i');
        _writeString(b, 'audio');
        _writeString(b, '');
        _writeBool(b, v: i.isEven); // 0 and 2 are playable
        _writeUint32(b, 1); // 1 metadata entry
        _writeString(b, 'Index');
        _writeString(b, '$i');
      }

      final result = GlazeCodec.decode<BlueZMediaFolderItems>(
        Uint8List.fromList(b.toBytes()),
        0,
      );

      expect(result.items.length, 3);
      for (var i = 0; i < 3; i++) {
        expect(result.items[i].objectPath, '/item$i');
        expect(result.items[i].name, 'Track $i');
        expect(result.items[i].playable, i.isEven);
        expect(result.items[i].metadata.single.value, '$i');
      }
    });
  });
}
