import 'dart:async';

import 'bluez_media_client.dart' show BluezMediaClient;
import 'bluez_media_item.dart';
import 'ffi/types.dart';

/// Proxy for a remote `org.bluez.MediaFolder1` object.
class BluezMediaFolder {
  final BluezMediaClient _client;
  bool _disposed = false;
  int _revision = 0;

  bool get isDisposed => _disposed;

  BluezMediaClient get _activeClient {
    if (_disposed) throw StateError('Media proxy has been disposed.');
    return _client;
  }

  BlueZMediaFolderProps _props;

  final _propertiesChangedCtrl = StreamController<List<String>>.broadcast();

  BluezMediaFolder.internal(this._client, String objectPath)
    : _props = BlueZMediaFolderProps(objectPath: objectPath);

  /// D-Bus object path.
  String get objectPath => _props.objectPath;

  int get numberOfItems => _props.numberOfItems;
  String get name => _props.name;

  /// Emits property names after [refresh] or future native event routing.
  Stream<List<String>> get propertiesChanged => _propertiesChangedCtrl.stream;

  /// Fetch the latest folder snapshot from BlueZ.
  Future<BlueZMediaFolderProps> refresh() async {
    final revision = ++_revision;
    final properties = await _activeClient.getMediaFolderProperties(objectPath);
    if (revision == _revision) updateProps(properties);
    return _props;
  }

  /// Search this folder and return the result folder proxy.
  Future<BluezMediaFolder> search(String value) async {
    final props = await _activeClient.searchFolder(objectPath, value);
    return _activeClient.folder(props.objectPath)..updateProps(props);
  }

  /// List child items/folders under this folder.
  Future<List<BluezMediaItem>> listItems() async {
    final result = await _activeClient.listFolderItems(objectPath);
    return result.items
        .map(_activeClient.itemFromProps)
        .toList(growable: false);
  }

  /// Change this player folder to [targetFolder].
  Future<void> changeFolder(BluezMediaFolder targetFolder) =>
      _activeClient.changeFolder(objectPath, targetFolder.objectPath);

  /// Change this player folder to [targetFolderPath].
  Future<void> changeFolderPath(String targetFolderPath) =>
      _activeClient.changeFolder(objectPath, targetFolderPath);

  void updateProps(BlueZMediaFolderProps props) {
    if (_disposed) return;
    _revision++;
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
    if (_disposed) return;
    _disposed = true;
    _propertiesChangedCtrl.close();
  }
}
