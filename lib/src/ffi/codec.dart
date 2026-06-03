// codec.dart — GlazeCodec for decoding BlueZ Media native payloads.
// Matches the binary encoding in glaze_meta.h (little-endian, length-prefixed).

import 'dart:convert';
import 'dart:typed_data';

import 'types.dart';

/// Decodes glaze binary payloads from the native bridge.
class GlazeCodec {
  GlazeCodec._();

  static T decode<T>(Uint8List data, int offset) {
    final r = _Reader(data, offset);

    if (T == BlueZMediaProperty) {
      return _decodeMediaProperty(r) as T;
    } else if (T == BlueZMediaPlayerProps) {
      return _decodeMediaPlayerProps(r) as T;
    } else if (T == BlueZMediaControlProps) {
      return _decodeMediaControlProps(r) as T;
    } else if (T == BlueZMediaEndpointProps) {
      return _decodeMediaEndpointProps(r) as T;
    } else if (T == BlueZMediaTransportProps) {
      return _decodeMediaTransportProps(r) as T;
    } else if (T == BlueZMediaFolderProps) {
      return _decodeMediaFolderProps(r) as T;
    } else if (T == BlueZMediaItemProps) {
      return _decodeMediaItemProps(r) as T;
    } else if (T == BlueZMediaAcquireResult) {
      return _decodeMediaAcquireResult(r) as T;
    } else if (T == BlueZMediaError) {
      return _decodeMediaError(r) as T;
    }
    throw ArgumentError('Unknown type: $T');
  }

  static BlueZMediaProperty _decodeMediaProperty(_Reader r) {
    return BlueZMediaProperty(key: r.readString(), value: r.readString());
  }

  static BlueZMediaPlayerProps _decodeMediaPlayerProps(_Reader r) {
    return BlueZMediaPlayerProps(
      objectPath: r.readString(),
      changedMask: r.readUint32(),
      equalizer: r.readString(),
      repeat: r.readString(),
      shuffle: r.readString(),
      scan: r.readString(),
      status: r.readString(),
      position: r.readUint32(),
      track: r.readMediaPropertyList(),
      device: r.readString(),
      name: r.readString(),
      type: r.readString(),
      subtype: r.readString(),
      browsable: r.readBool(),
      searchable: r.readBool(),
      playlist: r.readString(),
    );
  }

  static BlueZMediaControlProps _decodeMediaControlProps(_Reader r) {
    return BlueZMediaControlProps(
      objectPath: r.readString(),
      changedMask: r.readUint32(),
      connected: r.readBool(),
      player: r.readString(),
    );
  }

  static BlueZMediaEndpointProps _decodeMediaEndpointProps(_Reader r) {
    return BlueZMediaEndpointProps(
      objectPath: r.readString(),
      uuid: r.readString(),
      codec: r.readUint8(),
      capabilities: r.readByteList(),
      device: r.readString(),
      delayReporting: r.readBool(),
    );
  }

  static BlueZMediaTransportProps _decodeMediaTransportProps(_Reader r) {
    return BlueZMediaTransportProps(
      objectPath: r.readString(),
      device: r.readString(),
      uuid: r.readString(),
      codec: r.readUint8(),
      configuration: r.readByteList(),
      state: r.readString(),
      delay: r.readUint16(),
      volume: r.readUint16(),
      endpoint: r.readString(),
    );
  }

  static BlueZMediaFolderProps _decodeMediaFolderProps(_Reader r) {
    return BlueZMediaFolderProps(
      objectPath: r.readString(),
      numberOfItems: r.readUint32(),
      name: r.readString(),
    );
  }

  static BlueZMediaItemProps _decodeMediaItemProps(_Reader r) {
    return BlueZMediaItemProps(
      objectPath: r.readString(),
      player: r.readString(),
      name: r.readString(),
      type: r.readString(),
      folderType: r.readString(),
      playable: r.readBool(),
      metadata: r.readMediaPropertyList(),
    );
  }

  static BlueZMediaAcquireResult _decodeMediaAcquireResult(_Reader r) {
    return BlueZMediaAcquireResult(
      transportPath: r.readString(),
      fd: r.readUint64(),
      readMtu: r.readUint16(),
      writeMtu: r.readUint16(),
    );
  }

  static BlueZMediaError _decodeMediaError(_Reader r) {
    return BlueZMediaError(
      objectPath: r.readString(),
      name: r.readString(),
      message: r.readString(),
    );
  }
}

class _Reader {
  final ByteData _data;
  final int _length;
  int _offset;

  _Reader(Uint8List bytes, int offset)
    : _data = bytes.buffer.asByteData(bytes.offsetInBytes),
      _length = bytes.length,
      _offset = offset;

  void _checkBounds(int needed) {
    if (_offset + needed > _length) {
      throw RangeError(
        'Codec read overrun: need $needed bytes at offset $_offset, '
        'but buffer is $_length bytes',
      );
    }
  }

  bool readBool() {
    _checkBounds(1);
    final v = _data.getUint8(_offset) != 0;
    _offset += 1;
    return v;
  }

  int readUint8() {
    _checkBounds(1);
    final v = _data.getUint8(_offset);
    _offset += 1;
    return v;
  }

  int readUint16() {
    _checkBounds(2);
    final v = _data.getUint16(_offset, Endian.little);
    _offset += 2;
    return v;
  }

  int readUint32() {
    _checkBounds(4);
    final v = _data.getUint32(_offset, Endian.little);
    _offset += 4;
    return v;
  }

  int readUint64() {
    _checkBounds(8);
    final v = _data.getUint64(_offset, Endian.little);
    _offset += 8;
    return v;
  }

  String readString() {
    final len = readUint32();
    _checkBounds(len);
    final bytes = Uint8List.view(
      _data.buffer,
      _data.offsetInBytes + _offset,
      len,
    );
    _offset += len;
    return utf8.decode(bytes);
  }

  List<int> readByteList() {
    final count = readUint32();
    _checkBounds(count);
    final bytes = List<int>.from(
      Uint8List.view(_data.buffer, _data.offsetInBytes + _offset, count),
    );
    _offset += count;
    return bytes;
  }

  List<BlueZMediaProperty> readMediaPropertyList() {
    final count = readUint32();
    return List.generate(
      count,
      (_) => BlueZMediaProperty(key: readString(), value: readString()),
    );
  }
}
