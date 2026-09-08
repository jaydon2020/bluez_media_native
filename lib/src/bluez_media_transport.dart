import 'dart:async';
import 'dart:ffi';

import 'bluez_media_client.dart';

/// A proxy for a remote `org.bluez.MediaTransport1` object.
class BluezMediaTransport {
  final BluezMediaClient _client;
  bool _disposed = false;

  bool get isDisposed => _disposed;

  BluezMediaClient get _activeClient {
    if (_disposed) throw StateError('Media proxy has been disposed.');
    return _client;
  }

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
  List<int> get configuration => List.unmodifiable(props.configuration);

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
  Future<void> setVolume(int value) async {
    await _activeClient.transportSetVolume(objectPath, value);
    await refresh();
  }

  /// Acquires the transport file descriptor.
  Future<BluezMediaAcquiredTransport> acquire() async {
    return BluezMediaAcquiredTransport._(
      _client,
      await _activeClient.transportAcquire(objectPath),
    );
  }

  /// Tries to acquire the transport file descriptor without blocking.
  Future<BluezMediaAcquiredTransport> tryAcquire() async {
    return BluezMediaAcquiredTransport._(
      _client,
      await _activeClient.transportTryAcquire(objectPath),
    );
  }

  /// Releases the transport file descriptor.
  Future<void> release() => _activeClient.transportRelease(objectPath);

  /// Fetches the latest properties from BlueZ and updates the snapshot.
  Future<void> refresh() async {
    updateProps(await _activeClient.getMediaTransportProperties(objectPath));
  }

  void updateProps(BlueZMediaTransportProps props) {
    if (_disposed) return;
    final changed = <String>[];
    final previous = _props ?? BlueZMediaTransportProps(objectPath: objectPath);
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
    _props = props;
    if (changed.isNotEmpty) {
      _propertiesChangedCtrl.add(changed);
    }
  }

  void dispose() {
    if (_disposed) return;
    _disposed = true;
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
class BluezMediaAcquiredTransport implements Finalizable {
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
    // Linux releases the descriptor even on most close errors. Never retry a
    // numeric fd that another thread may already have reused.
    _closed = true;
    _client.closeFileDescriptor(fd);
  }
}
