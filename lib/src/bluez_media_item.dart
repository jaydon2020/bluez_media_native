import 'dart:async';

import 'bluez_media_client.dart' show BluezMediaClient;
import 'ffi/types.dart';

/// Proxy for a remote `org.bluez.MediaItem1` object.
class BluezMediaItem {
  final BluezMediaClient _client;
  BlueZMediaItemProps _props;

  final _propertiesChangedCtrl = StreamController<List<String>>.broadcast();

  BluezMediaItem.internal(this._client, String objectPath)
    : _props = BlueZMediaItemProps(objectPath: objectPath);

  BluezMediaItem.fromProps(this._client, BlueZMediaItemProps props)
    : _props = props;

  /// D-Bus object path.
  String get objectPath => _props.objectPath;

  String get playerPath => _props.player;
  String get name => _props.name;
  String get type => _props.type;
  String get folderType => _props.folderType;
  bool get playable => _props.playable;
  List<BlueZMediaProperty> get metadata => List.unmodifiable(_props.metadata);

  /// Emits property names after [refresh] or future native event routing.
  Stream<List<String>> get propertiesChanged => _propertiesChangedCtrl.stream;

  void play() => _client.playItem(objectPath);
  void addToNowPlaying() => _client.addItemToNowPlaying(objectPath);

  /// Fetch the latest item snapshot from BlueZ.
  BlueZMediaItemProps refresh() {
    updateProps(_client.getMediaItemProperties(objectPath));
    return _props;
  }

  void updateProps(BlueZMediaItemProps props) {
    final changed = <String>[];
    if (props.player != _props.player) changed.add('Player');
    if (props.name != _props.name) changed.add('Name');
    if (props.type != _props.type) changed.add('Type');
    if (props.folderType != _props.folderType) changed.add('FolderType');
    if (props.playable != _props.playable) changed.add('Playable');
    if (!_sameProperties(props.metadata, _props.metadata)) {
      changed.add('Metadata');
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
