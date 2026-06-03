import 'dart:async';

import 'bluez_media_client.dart' show BluezMediaClient;
import 'ffi/types.dart';

/// Proxy for a remote `org.bluez.MediaControl1` object.
class BluezMediaControl {
  final BluezMediaClient _client;
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

  void play() => _client.controlPlay(objectPath);
  void pause() => _client.controlPause(objectPath);
  void stop() => _client.controlStop(objectPath);
  void next() => _client.controlNext(objectPath);
  void previous() => _client.controlPrevious(objectPath);
  void volumeUp() => _client.volumeUp(objectPath);
  void volumeDown() => _client.volumeDown(objectPath);
  void fastForward() => _client.fastForward(objectPath);
  void rewind() => _client.rewind(objectPath);

  /// Fetch the latest control snapshot from BlueZ.
  BlueZMediaControlProps refresh() {
    updateProps(_client.getMediaControlProperties(objectPath));
    return _props;
  }

  void updateProps(BlueZMediaControlProps props) {
    final changed = <String>[];
    if (props.connected != _props.connected) changed.add('Connected');
    if (props.player != _props.player) changed.add('Player');

    _props = props;
    if (changed.isNotEmpty) {
      _propertiesChangedCtrl.add(changed);
    }
  }

  void dispose() {
    _propertiesChangedCtrl.close();
  }
}
