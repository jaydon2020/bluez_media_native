import 'dart:async';

import 'bluez_media_client.dart' show BluezMediaClient;
import 'ffi/types.dart';

/// Proxy for a remote `org.bluez.MediaControl1` object.
class BluezMediaControl {
  final BluezMediaClient _client;
  bool _disposed = false;

  bool get isDisposed => _disposed;

  BluezMediaClient get _activeClient {
    if (_disposed) throw StateError('Media proxy has been disposed.');
    return _client;
  }

  BlueZMediaControlProps _props;

  final _propertiesChangedCtrl = StreamController<List<String>>.broadcast();

  BluezMediaControl.internal(this._client, String objectPath)
    : _props = BlueZMediaControlProps(objectPath: objectPath);

  /// D-Bus object path.
  String get objectPath => _props.objectPath;

  /// Whether the remote controller is connected.
  bool get connected => _props.connected;

  /// Active player path reported by BlueZ.
  String get playerPath => _props.player;

  /// Emits property names after [refresh] or future native event routing.
  Stream<List<String>> get propertiesChanged => _propertiesChangedCtrl.stream;

  Future<void> play() => _activeClient.controlPlay(objectPath);
  Future<void> pause() => _activeClient.controlPause(objectPath);
  Future<void> stop() => _activeClient.controlStop(objectPath);
  Future<void> next() => _activeClient.controlNext(objectPath);
  Future<void> previous() => _activeClient.controlPrevious(objectPath);
  Future<void> volumeUp() => _activeClient.volumeUp(objectPath);
  Future<void> volumeDown() => _activeClient.volumeDown(objectPath);
  Future<void> fastForward() => _activeClient.fastForward(objectPath);
  Future<void> rewind() => _activeClient.rewind(objectPath);

  /// Fetch the latest control snapshot from BlueZ.
  Future<BlueZMediaControlProps> refresh() async {
    updateProps(await _activeClient.getMediaControlProperties(objectPath));
    return _props;
  }

  void updateProps(BlueZMediaControlProps props) {
    if (_disposed) return;
    final changed = <String>[];
    if (props.connected != _props.connected) changed.add('Connected');
    if (props.player != _props.player) changed.add('Player');

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
