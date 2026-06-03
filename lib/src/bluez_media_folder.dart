import 'dart:async';

import 'bluez_media_client.dart' show BluezMediaClient;
import 'bluez_media_item.dart';
import 'ffi/types.dart';

/// Proxy for a remote `org.bluez.MediaFolder1` object.
class BluezMediaFolder {
  final BluezMediaClient _client;
  BlueZMediaFolderProps _props;

  final _propertiesChangedCtrl = StreamController<List<String>>.broadcast();

  BluezMediaFolder.internal(this._client, String objectPath)
    : _props = BlueZMediaFolderProps(objectPath: objectPath);

  BluezMediaFolder.fromProps(this._client, BlueZMediaFolderProps props)
    : _props = props;

  /// D-Bus object path.
  String get objectPath => _props.objectPath;

  int get numberOfItems => _props.numberOfItems;
  String get name => _props.name;

  /// Emits property names after [refresh] or future native event routing.
  Stream<List<String>> get propertiesChanged => _propertiesChangedCtrl.stream;

  /// Fetch the latest folder snapshot from BlueZ.
  BlueZMediaFolderProps refresh() {
    updateProps(_client.getMediaFolderProperties(objectPath));
    return _props;
  }

  /// Search this folder and return the result folder proxy.
  BluezMediaFolder search(String value) {
    final props = _client.searchFolder(objectPath, value);
    return _client.folder(props.objectPath)..updateProps(props);
  }

  /// List child items/folders under this folder.
  List<BluezMediaItem> listItems() {
    final result = _client.listFolderItems(objectPath);
    return result.items.map(_client.itemFromProps).toList(growable: false);
  }

  /// Change this player folder to [targetFolder].
  void changeFolder(BluezMediaFolder targetFolder) {
    _client.changeFolder(objectPath, targetFolder.objectPath);
  }

  /// Change this player folder to [targetFolderPath].
  void changeFolderPath(String targetFolderPath) {
    _client.changeFolder(objectPath, targetFolderPath);
  }

  void updateProps(BlueZMediaFolderProps props) {
    final changed = <String>[];
    if (props.numberOfItems != _props.numberOfItems) {
      changed.add('NumberOfItems');
    }
    if (props.name != _props.name) changed.add('Name');

    _props = props;
    if (changed.isNotEmpty) {
      _propertiesChangedCtrl.add(changed);
    }
  }

  void dispose() {
    _propertiesChangedCtrl.close();
  }
}
