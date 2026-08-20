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
  String get equalizer => _props.equalizer;
  String get scan => _props.scan;

  /// Current playback position in milliseconds.
  int get position => _props.position;

  /// Current track metadata as string key/value properties.
  List<BlueZMediaProperty> get track => List.unmodifiable(_props.track);

  /// AVRCP image handle for the current track, when cover art is available.
  String get imageHandle {
    for (final property in _props.track) {
      if (property.key == 'ImgHandle') return property.value;
    }
    return '';
  }

  String get name => _props.name;
  String get type => _props.type;
  String get subtype => _props.subtype;
  String get device => _props.device;
  String get repeat => _props.repeat;
  String get shuffle => _props.shuffle;
  bool get browsable => _props.browsable;
  bool get searchable => _props.searchable;
  String get playlist => _props.playlist;
  int get obexPort => _props.obexPort;

  /// Emits property names after [refresh] or future native event routing.
  Stream<List<String>> get propertiesChanged => _propertiesChangedCtrl.stream;

  Future<void> play() => _client.play(objectPath);
  Future<void> pause() => _client.pause(objectPath);
  Future<void> stop() => _client.stop(objectPath);
  Future<void> next() => _client.next(objectPath);
  Future<void> previous() => _client.previous(objectPath);
  Future<void> fastForward() => _client.playerFastForward(objectPath);
  Future<void> rewind() => _client.playerRewind(objectPath);
  Future<void> setRepeat(String repeat) =>
      _client.setRepeat(objectPath, repeat);
  Future<void> setShuffle(String shuffle) =>
      _client.setShuffle(objectPath, shuffle);

  /// Downloads the current track's cover art through BlueZ OBEX BIP.
  ///
  /// [targetFile] must be an absolute path that does not already exist.
  Future<String> getCoverArt(
    String targetFile, {
    Duration timeout = const Duration(seconds: 15),
  }) {
    return _client.getPlayerCoverArt(objectPath, targetFile, timeout: timeout);
  }

  /// Fetch the latest player snapshot from BlueZ.
  Future<BlueZMediaPlayerProps> refresh() async {
    updateProps(await _client.getPlayerProperties(objectPath));
    return _props;
  }

  void updateProps(BlueZMediaPlayerProps props) {
    final changed = <String>[];
    if (props.equalizer != _props.equalizer) changed.add('Equalizer');
    if (props.status != _props.status) changed.add('Status');
    if (props.position != _props.position) changed.add('Position');
    if (!_sameProperties(props.track, _props.track)) changed.add('Track');
    if (props.repeat != _props.repeat) changed.add('Repeat');
    if (props.shuffle != _props.shuffle) changed.add('Shuffle');
    if (props.scan != _props.scan) changed.add('Scan');
    if (props.name != _props.name) changed.add('Name');
    if (props.type != _props.type) changed.add('Type');
    if (props.subtype != _props.subtype) changed.add('Subtype');
    if (props.device != _props.device) changed.add('Device');
    if (props.browsable != _props.browsable) changed.add('Browsable');
    if (props.searchable != _props.searchable) changed.add('Searchable');
    if (props.playlist != _props.playlist) changed.add('Playlist');
    if (props.obexPort != _props.obexPort) changed.add('ObexPort');

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
