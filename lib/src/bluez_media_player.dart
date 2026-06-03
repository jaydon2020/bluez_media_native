import 'dart:async';

import 'bluez_media_client.dart' show BluezMediaClient;
import 'ffi/types.dart';

/// Proxy for a remote `org.bluez.MediaPlayer1` object.
class BluezMediaPlayer {
  final BluezMediaClient _client;
  BlueZMediaPlayerProps _props;

  final _propertiesChangedCtrl = StreamController<List<String>>.broadcast();

  BluezMediaPlayer.internal(this._client, String objectPath)
    : _props = BlueZMediaPlayerProps(objectPath: objectPath);

  /// D-Bus object path.
  String get objectPath => _props.objectPath;

  /// Current playback status, such as `playing`, `paused`, or `stopped`.
  String get status => _props.status;

  /// Current playback position in milliseconds.
  int get position => _props.position;

  /// Current track metadata as string key/value properties.
  List<BlueZMediaProperty> get track => List.unmodifiable(_props.track);

  String get name => _props.name;
  String get type => _props.type;
  String get subtype => _props.subtype;
  String get device => _props.device;
  bool get browsable => _props.browsable;
  bool get searchable => _props.searchable;
  String get playlist => _props.playlist;

  /// Emits property names after [refresh] or future native event routing.
  Stream<List<String>> get propertiesChanged => _propertiesChangedCtrl.stream;

  void play() => _client.play(objectPath);
  void pause() => _client.pause(objectPath);
  void stop() => _client.stop(objectPath);
  void next() => _client.next(objectPath);
  void previous() => _client.previous(objectPath);

  /// Fetch the latest player snapshot from BlueZ.
  BlueZMediaPlayerProps refresh() {
    updateProps(_client.getPlayerProperties(objectPath));
    return _props;
  }

  void updateProps(BlueZMediaPlayerProps props) {
    final changed = <String>[];
    if (props.status != _props.status) changed.add('Status');
    if (props.position != _props.position) changed.add('Position');
    if (!_sameProperties(props.track, _props.track)) changed.add('Track');
    if (props.name != _props.name) changed.add('Name');
    if (props.device != _props.device) changed.add('Device');

    _props = props;
    if (changed.isNotEmpty) {
      _propertiesChangedCtrl.add(changed);
    }
  }

  void dispose() {
    _propertiesChangedCtrl.close();
  }
}

bool _sameProperties(
  List<BlueZMediaProperty> left,
  List<BlueZMediaProperty> right,
) {
  if (left.length != right.length) return false;
  for (var i = 0; i < left.length; i++) {
    if (left[i].key != right[i].key || left[i].value != right[i].value) {
      return false;
    }
  }
  return true;
}
