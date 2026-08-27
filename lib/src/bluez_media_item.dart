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

  /// D-Bus object path.
  String get objectPath => _props.objectPath;

  String get playerPath => _props.player;
  String get name => _props.name;
  String get type => _props.type;
  String get folderType => _props.folderType;
  bool get playable => _props.playable;
  List<BlueZMediaProperty> get metadata => List.unmodifiable(_props.metadata);

  /// AVRCP image handle exposed by experimental or vendor `MediaItem1`
  /// metadata, when available.
  String get imageHandle {
    for (final property in _props.metadata) {
      if (property.key == 'ImgHandle') return property.value;
    }
    return '';
  }

  /// Emits property names after [refresh] or future native event routing.
  Stream<List<String>> get propertiesChanged => _propertiesChangedCtrl.stream;

  Future<void> play() => _client.playItem(objectPath);
  Future<void> addToNowPlaying() => _client.addItemToNowPlaying(objectPath);

  /// Downloads the item's cover art through BlueZ OBEX BIP.
  ///
  /// [targetFile] must be an absolute path that does not already exist.
  Future<String> getCoverArt(
    String targetFile, {
    Duration timeout = const Duration(seconds: 15),
  }) {
    return _client.getItemCoverArt(objectPath, targetFile, timeout: timeout);
  }

  /// Downloads cover art through an existing OBEX BIP session.
  Future<String> getCoverArtFromExistingSession(
    String targetFile, {
    Duration timeout = const Duration(seconds: 15),
  }) {
    return _client.getItemCoverArtFromExistingSession(
      objectPath,
      targetFile,
      timeout: timeout,
    );
  }

  /// Fetch the latest item snapshot from BlueZ.
  Future<BlueZMediaItemProps> refresh() async {
    updateProps(await _client.getMediaItemProperties(objectPath));
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
