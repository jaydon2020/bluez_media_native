import 'dart:async';

import 'bluez_media_client.dart';

/// A proxy for a remote `org.bluez.MediaTransport1` object.
class BluezMediaTransport {
  final BluezMediaClient _client;
  final String objectPath;
  BlueZMediaTransportProps? _props;
  final _propertiesChangedCtrl = StreamController<List<String>>.broadcast();

  /// Do not instantiate directly. Use [BluezMediaClient.transport].
  BluezMediaTransport.internal(this._client, this.objectPath);

  /// Latest cached transport properties.
  ///
  /// The cache is populated by ObjectManager events. Call [refresh] to request
  /// an immediate synchronous snapshot.
  BlueZMediaTransportProps get props =>
      _props ?? BlueZMediaTransportProps(objectPath: objectPath);

  /// The UUID of the profile that the transport is for.
  String get uuid => props.uuid;

  /// The codec of the transport.
  int get codec => props.codec;

  /// The configuration of the transport.
  List<int> get configuration => props.configuration;

  /// The state of the transport.
  String get state => props.state;

  /// The delay of the transport in 1/10 of millisecond.
  int get delay => props.delay;

  /// The endpoint associated with this transport.
  String get endpoint => props.endpoint;

  /// The device associated with this transport.
  String get device => props.device;

  /// The volume of the transport.
  int get volume => props.volume;

  /// Emits property names whenever BlueZ updates this transport.
  Stream<List<String>> get propertiesChanged => _propertiesChangedCtrl.stream;

  /// Sets the volume of the transport.
  /// Automatically refreshes the property snapshot after the update.
  set volume(int value) {
    _client.transportSetVolume(objectPath, value);
    refresh();
  }

  /// Acquires the transport file descriptor.
  BluezMediaAcquiredTransport acquire() {
    return BluezMediaAcquiredTransport._(
      _client,
      _client.transportAcquire(objectPath),
    );
  }

  /// Tries to acquire the transport file descriptor without blocking.
  BluezMediaAcquiredTransport tryAcquire() {
    return BluezMediaAcquiredTransport._(
      _client,
      _client.transportTryAcquire(objectPath),
    );
  }

  /// Releases the transport file descriptor.
  void release() {
    _client.transportRelease(objectPath);
  }

  /// Fetches the latest properties from BlueZ and updates the snapshot.
  void refresh() {
    _props = _client.getMediaTransportProperties(objectPath);
  }

  void updateProps(BlueZMediaTransportProps props) {
    final changed = <String>[];
    if (_props case final previous?) {
      if (props.device != previous.device) changed.add('Device');
      if (props.uuid != previous.uuid) changed.add('UUID');
      if (props.codec != previous.codec) changed.add('Codec');
      if (props.state != previous.state) changed.add('State');
      if (props.delay != previous.delay) changed.add('Delay');
      if (props.volume != previous.volume) changed.add('Volume');
      if (!_sameInts(props.configuration, previous.configuration)) {
        changed.add('Configuration');
      }
      if (props.endpoint != previous.endpoint) changed.add('Endpoint');
    }
    _props = props;
    if (changed.isNotEmpty) {
      _propertiesChangedCtrl.add(changed);
    }
  }

  void dispose() {
    _propertiesChangedCtrl.close();
  }
}

bool _sameInts(List<int> left, List<int> right) {
  if (left.length != right.length) return false;
  for (var i = 0; i < left.length; i++) {
    if (left[i] != right[i]) return false;
  }
  return true;
}

/// Owns a duplicated MediaTransport file descriptor.
class BluezMediaAcquiredTransport {
  final BluezMediaClient _client;
  final BlueZMediaAcquireResult _result;
  bool _closed = false;

  BluezMediaAcquiredTransport._(this._client, this._result);

  String get transportPath => _result.transportPath;
  int get fd => _result.fd;
  int get readMtu => _result.readMtu;
  int get writeMtu => _result.writeMtu;
  bool get isClosed => _closed;

  /// Closes the duplicated file descriptor returned by BlueZ.
  void close() {
    if (_closed) return;
    _client.closeFileDescriptor(fd);
    _closed = true;
  }
}
