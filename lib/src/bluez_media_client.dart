import 'dart:async';
import 'dart:ffi';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import '../bluez_media_native_bindings_generated.dart';
import 'bluez_media_control.dart';
import 'bluez_media_folder.dart';
import 'bluez_media_item.dart';
import 'bluez_media_player.dart';
import 'bluez_media_transport.dart';
import 'exceptions.dart';
import 'ffi/codec.dart';
import 'ffi/types.dart';
import 'internal/library_loader.dart';
export 'bluez_media_control.dart';
export 'bluez_media_folder.dart';
export 'bluez_media_item.dart';
export 'bluez_media_player.dart';
export 'bluez_media_transport.dart';
export 'exceptions.dart';
export 'ffi/types.dart'
    show
        BlueZMediaAcquireResult,
        BlueZMediaControlProps,
        BlueZMediaFolderItems,
        BlueZMediaFolderProps,
        BlueZMediaItemProps,
        BlueZMediaManagedObjects,
        BlueZMediaObjectRemoved,
        BlueZMediaPlayerProps,
        BlueZMediaTransportProps,
        BlueZMediaProperty;

class BluezMediaPlayerRegistrationConfig {
  final String adapterPath;
  final String playerPath;
  final String name;
  final String type;
  final String subtype;
  final bool browsable;
  final bool searchable;

  const BluezMediaPlayerRegistrationConfig({
    required this.adapterPath,
    required this.playerPath,
    this.name = 'bluez_media_native',
    this.type = 'Audio',
    this.subtype = '',
    this.browsable = false,
    this.searchable = false,
  });
}

class BluezMediaClient implements Finalizable {
  static final _finalizer = NativeFinalizer(
    _dylib.lookup<NativeFunction<Void Function(Pointer<Void>)>>(
      'bluez_media_client_destroy',
    ),
  );
  Pointer<Void> _handle;
  bool _closed = false;
  bool _serviceAvailable = true;
  final _serviceAvailabilityCtrl = StreamController<bool>.broadcast();

  /// Whether BlueZ has a current owner and its object snapshot is available.
  bool get isServiceAvailable => !_closed && _serviceAvailable;

  /// Emits false on owner loss and true after the replacement snapshot arrives.
  /// Local registrations are invalidated on owner/adapter loss. Register them
  /// again when the service and adapter are available.
  Stream<bool> get serviceAvailabilityChanged =>
      _serviceAvailabilityCtrl.stream;
  final _players = <String, BluezMediaPlayer>{};
  final _controls = <String, BluezMediaControl>{};
  final _folders = <String, BluezMediaFolder>{};
  final _items = <String, BluezMediaItem>{};
  final _transports = <String, BluezMediaTransport>{};
  final _transportAddedCtrl = StreamController<BluezMediaTransport>.broadcast();
  final _transportRemovedCtrl =
      StreamController<BluezMediaTransport>.broadcast();
  final _playerAddedCtrl = StreamController<BluezMediaPlayer>.broadcast();
  final _playerRemovedCtrl = StreamController<BluezMediaPlayer>.broadcast();
  final _controlAddedCtrl = StreamController<BluezMediaControl>.broadcast();
  final _controlRemovedCtrl = StreamController<BluezMediaControl>.broadcast();
  final _folderAddedCtrl = StreamController<BluezMediaFolder>.broadcast();
  final _folderRemovedCtrl = StreamController<BluezMediaFolder>.broadcast();
  final _itemAddedCtrl = StreamController<BluezMediaItem>.broadcast();
  final _itemRemovedCtrl = StreamController<BluezMediaItem>.broadcast();
  final _ready = Completer<void>();
  ReceivePort? _eventsPort;

  BluezMediaClient._() : _handle = nullptr {
    _eventsPort = ReceivePort('bluez_media.events');
    _eventsPort!.listen(_onEvent);
  }

  /// Connects to BlueZ and returns after the initial object snapshot is ready.
  static Future<BluezMediaClient> create() async {
    _initializeNativeApi();
    final client = BluezMediaClient._();
    final resultPort = ReceivePort('bluez_media.connect');
    _bindings.bluez_media_client_create_async(
      client._eventsPort!.sendPort.nativePort,
      resultPort.sendPort.nativePort,
    );
    final result = await resultPort.first;
    resultPort.close();
    if (result case final int address when address != 0) {
      client._handle = Pointer<Void>.fromAddress(address);
      _finalizer.attach(client, client._handle, detach: client);
      await client.ready;
      return client;
    }
    client._eventsPort?.close();
    client._eventsPort = null;
    throw _exceptionFromResult(result, serviceUnavailable: true);
  }

  Future<void> close() async {
    if (_closed) return;
    _closed = true;
    _finalizer.detach(this);
    if (_handle != nullptr) _bindings.bluez_media_client_destroy(_handle);
    _handle = nullptr;
    _eventsPort?.close();
    _eventsPort = null;
    for (final player in _players.values) {
      player.dispose();
    }
    for (final control in _controls.values) {
      control.dispose();
    }
    for (final folder in _folders.values) {
      folder.dispose();
    }
    for (final item in _items.values) {
      item.dispose();
    }
    for (final transport in _transports.values) {
      transport.dispose();
    }
    _players.clear();
    _controls.clear();
    _folders.clear();
    _items.clear();
    _transports.clear();
    await Future.wait([
      _serviceAvailabilityCtrl.close(),
      _transportAddedCtrl.close(),
      _transportRemovedCtrl.close(),
      _playerAddedCtrl.close(),
      _playerRemovedCtrl.close(),
      _controlAddedCtrl.close(),
      _controlRemovedCtrl.close(),
      _folderAddedCtrl.close(),
      _folderRemovedCtrl.close(),
      _itemAddedCtrl.close(),
      _itemRemovedCtrl.close(),
    ]);
  }

  List<BluezMediaPlayer> get players => List.unmodifiable(_players.values);
  List<BluezMediaControl> get controls => List.unmodifiable(_controls.values);
  List<BluezMediaFolder> get folders => List.unmodifiable(_folders.values);
  List<BluezMediaItem> get items => List.unmodifiable(_items.values);
  List<BluezMediaTransport> get transports =>
      List.unmodifiable(_transports.values);

  /// Completes after the initial BlueZ ObjectManager snapshot is cached.
  Future<void> get ready => _ready.future;

  Stream<BluezMediaTransport> get transportAdded => _transportAddedCtrl.stream;
  Stream<BluezMediaTransport> get transportRemoved =>
      _transportRemovedCtrl.stream;
  Stream<BluezMediaPlayer> get playerAdded => _playerAddedCtrl.stream;
  Stream<BluezMediaPlayer> get playerRemoved => _playerRemovedCtrl.stream;
  Stream<BluezMediaControl> get controlAdded => _controlAddedCtrl.stream;
  Stream<BluezMediaControl> get controlRemoved => _controlRemovedCtrl.stream;
  Stream<BluezMediaFolder> get folderAdded => _folderAddedCtrl.stream;
  Stream<BluezMediaFolder> get folderRemoved => _folderRemovedCtrl.stream;
  Stream<BluezMediaItem> get itemAdded => _itemAddedCtrl.stream;
  Stream<BluezMediaItem> get itemRemoved => _itemRemovedCtrl.stream;

  /// Return a cached proxy for a remote `org.bluez.MediaPlayer1` object.
  BluezMediaPlayer player(String objectPath) {
    _ensureOpen();
    return _player(objectPath);
  }

  BluezMediaPlayer _player(String objectPath) {
    return _players.putIfAbsent(
      objectPath,
      () => BluezMediaPlayer.internal(this, objectPath),
    );
  }

  /// Return a cached proxy for a remote `org.bluez.MediaControl1` object.
  BluezMediaControl control(String objectPath) {
    _ensureOpen();
    return _control(objectPath);
  }

  BluezMediaControl _control(String objectPath) {
    return _controls.putIfAbsent(
      objectPath,
      () => BluezMediaControl.internal(this, objectPath),
    );
  }

  /// Return a cached proxy for a remote `org.bluez.MediaFolder1` object.
  BluezMediaFolder folder(String objectPath) {
    _ensureOpen();
    return _folder(objectPath);
  }

  BluezMediaFolder _folder(String objectPath) {
    return _folders.putIfAbsent(
      objectPath,
      () => BluezMediaFolder.internal(this, objectPath),
    );
  }

  /// Return a cached proxy for a remote `org.bluez.MediaItem1` object.
  BluezMediaItem item(String objectPath) {
    _ensureOpen();
    return _item(objectPath);
  }

  BluezMediaItem _item(String objectPath) {
    return _items.putIfAbsent(
      objectPath,
      () => BluezMediaItem.internal(this, objectPath),
    );
  }

  /// Return a cached proxy for a remote `org.bluez.MediaTransport1` object.
  BluezMediaTransport transport(String objectPath) {
    _ensureOpen();
    return _transport(objectPath);
  }

  BluezMediaTransport _transport(String objectPath) {
    return _transports.putIfAbsent(
      objectPath,
      () => BluezMediaTransport.internal(this, objectPath),
    );
  }

  BluezMediaFolder folderFromProps(BlueZMediaFolderProps props) {
    return folder(props.objectPath)..updateProps(props);
  }

  BluezMediaItem itemFromProps(BlueZMediaItemProps props) {
    return item(props.objectPath)..updateProps(props);
  }

  /// Registers an experimental, inert MPRIS object for registration testing.
  /// It does not route commands to a Dart audio player or publish its metadata.
  /// Playback capabilities are false and remote commands return NotSupported.
  Future<void> registerPlayer(BluezMediaPlayerRegistrationConfig config) async {
    _ensureOpen();
    _validateRegistrationConfig(config);

    final registration = calloc<BluezMediaPlayerRegistration>();
    final strings = <Pointer<Utf8>>[];

    Pointer<Char> nativeString(String value) {
      final pointer = value.toNativeUtf8();
      strings.add(pointer);
      return pointer.cast<Char>();
    }

    try {
      registration.ref
        ..adapter_path = nativeString(config.adapterPath)
        ..player_path = nativeString(config.playerPath)
        ..name = nativeString(config.name)
        ..type = nativeString(config.type)
        ..subtype = nativeString(config.subtype)
        ..browsable = config.browsable ? 1 : 0
        ..searchable = config.searchable ? 1 : 0;

      final resultPort = ReceivePort('bluez_media.register_player');
      _bindings.bluez_media_register_player_async(
        _handle,
        registration,
        resultPort.sendPort.nativePort,
      );
      await _awaitNativeResult(resultPort);
    } finally {
      for (final pointer in strings) {
        calloc.free(pointer);
      }
      calloc.free(registration);
    }
  }

  Future<void> unregisterPlayer({
    required String adapterPath,
    required String playerPath,
  }) => _callAsync(
    BLUEZ_MEDIA_OP_UNREGISTER_PLAYER,
    objectPath: adapterPath,
    argument: playerPath,
  );

  Future<void> play(String playerPath) =>
      _callAsync(BLUEZ_MEDIA_OP_PLAYER_PLAY, objectPath: playerPath);

  Future<void> pause(String playerPath) =>
      _callAsync(BLUEZ_MEDIA_OP_PLAYER_PAUSE, objectPath: playerPath);

  Future<void> stop(String playerPath) =>
      _callAsync(BLUEZ_MEDIA_OP_PLAYER_STOP, objectPath: playerPath);

  Future<void> next(String playerPath) =>
      _callAsync(BLUEZ_MEDIA_OP_PLAYER_NEXT, objectPath: playerPath);

  Future<void> previous(String playerPath) =>
      _callAsync(BLUEZ_MEDIA_OP_PLAYER_PREVIOUS, objectPath: playerPath);

  Future<void> playerFastForward(String playerPath) =>
      _callAsync(BLUEZ_MEDIA_OP_PLAYER_FAST_FORWARD, objectPath: playerPath);

  Future<void> playerRewind(String playerPath) =>
      _callAsync(BLUEZ_MEDIA_OP_PLAYER_REWIND, objectPath: playerPath);

  Future<void> setRepeat(String playerPath, String repeat) {
    _checkMediaPlayerMode(repeat, _repeatModes, 'Repeat');
    return _callAsync(
      BLUEZ_MEDIA_OP_PLAYER_SET_REPEAT,
      objectPath: playerPath,
      argument: repeat,
    );
  }

  Future<void> setShuffle(String playerPath, String shuffle) {
    _checkMediaPlayerMode(shuffle, _shuffleModes, 'Shuffle');
    return _callAsync(
      BLUEZ_MEDIA_OP_PLAYER_SET_SHUFFLE,
      objectPath: playerPath,
      argument: shuffle,
    );
  }

  Future<BlueZMediaPlayerProps> getPlayerProperties(String playerPath) async {
    final payload = await _callAsync(
      BLUEZ_MEDIA_OP_PLAYER_GET_PROPERTIES,
      objectPath: playerPath,
    );
    return GlazeCodec.decode<BlueZMediaPlayerProps>(payload!, 0);
  }

  Future<String> getPlayerCoverArt(
    String playerPath,
    String targetFile, {
    Duration timeout = const Duration(seconds: 15),
  }) {
    _validateCoverArtRequest(targetFile, timeout);
    return _callAsync(
      BLUEZ_MEDIA_OP_PLAYER_GET_COVER_ART,
      objectPath: playerPath,
      argument: targetFile,
      value: timeout.inMilliseconds,
    ).then((_) => targetFile);
  }

  Future<String> getPlayerCoverArtFromExistingSession(
    String playerPath,
    String targetFile, {
    Duration timeout = const Duration(seconds: 15),
  }) {
    _validateCoverArtRequest(targetFile, timeout);
    return _callAsync(
      BLUEZ_MEDIA_OP_PLAYER_GET_COVER_ART_FROM_EXISTING_SESSION,
      objectPath: playerPath,
      argument: targetFile,
      value: timeout.inMilliseconds,
    ).then((_) => targetFile);
  }

  void _validateCoverArtRequest(String targetFile, Duration timeout) {
    _ensureOpen();
    if (!targetFile.startsWith('/')) {
      throw ArgumentError.value(targetFile, 'targetFile', 'Must be absolute.');
    }
    if (timeout <= Duration.zero || timeout.inMilliseconds > 0x7fffffff) {
      throw ArgumentError.value(
        timeout,
        'timeout',
        'Must fit a positive int32.',
      );
    }
  }

  Future<void> controlPlay(String controlPath) =>
      _callAsync(BLUEZ_MEDIA_OP_CONTROL_PLAY, objectPath: controlPath);

  Future<void> controlPause(String controlPath) =>
      _callAsync(BLUEZ_MEDIA_OP_CONTROL_PAUSE, objectPath: controlPath);

  Future<void> controlStop(String controlPath) =>
      _callAsync(BLUEZ_MEDIA_OP_CONTROL_STOP, objectPath: controlPath);

  Future<void> controlNext(String controlPath) =>
      _callAsync(BLUEZ_MEDIA_OP_CONTROL_NEXT, objectPath: controlPath);

  Future<void> controlPrevious(String controlPath) =>
      _callAsync(BLUEZ_MEDIA_OP_CONTROL_PREVIOUS, objectPath: controlPath);

  Future<void> volumeUp(String controlPath) =>
      _callAsync(BLUEZ_MEDIA_OP_CONTROL_VOLUME_UP, objectPath: controlPath);

  Future<void> volumeDown(String controlPath) =>
      _callAsync(BLUEZ_MEDIA_OP_CONTROL_VOLUME_DOWN, objectPath: controlPath);

  Future<void> fastForward(String controlPath) =>
      _callAsync(BLUEZ_MEDIA_OP_CONTROL_FAST_FORWARD, objectPath: controlPath);

  Future<void> rewind(String controlPath) =>
      _callAsync(BLUEZ_MEDIA_OP_CONTROL_REWIND, objectPath: controlPath);

  Future<BlueZMediaControlProps> getMediaControlProperties(
    String controlPath,
  ) async {
    final payload = await _callAsync(
      BLUEZ_MEDIA_OP_CONTROL_GET_PROPERTIES,
      objectPath: controlPath,
    );
    return GlazeCodec.decode<BlueZMediaControlProps>(payload!, 0);
  }

  Future<BlueZMediaFolderProps> searchFolder(
    String folderPath,
    String value,
  ) async {
    final payload = await _callAsync(
      BLUEZ_MEDIA_OP_FOLDER_SEARCH,
      objectPath: folderPath,
      argument: value,
    );
    return GlazeCodec.decode<BlueZMediaFolderProps>(payload!, 0);
  }

  Future<BlueZMediaFolderItems> listFolderItems(String folderPath) async {
    final payload = await _callAsync(
      BLUEZ_MEDIA_OP_FOLDER_LIST_ITEMS,
      objectPath: folderPath,
    );
    return GlazeCodec.decode<BlueZMediaFolderItems>(payload!, 0);
  }

  Future<void> changeFolder(String folderPath, String targetFolderPath) =>
      _callAsync(
        BLUEZ_MEDIA_OP_FOLDER_CHANGE_FOLDER,
        objectPath: folderPath,
        argument: targetFolderPath,
      );

  Future<BlueZMediaFolderProps> getMediaFolderProperties(
    String folderPath,
  ) async {
    final payload = await _callAsync(
      BLUEZ_MEDIA_OP_FOLDER_GET_PROPERTIES,
      objectPath: folderPath,
    );
    return GlazeCodec.decode<BlueZMediaFolderProps>(payload!, 0);
  }

  Future<void> playItem(String itemPath) =>
      _callAsync(BLUEZ_MEDIA_OP_ITEM_PLAY, objectPath: itemPath);

  Future<void> addItemToNowPlaying(String itemPath) =>
      _callAsync(BLUEZ_MEDIA_OP_ITEM_ADD_TO_NOW_PLAYING, objectPath: itemPath);

  Future<BlueZMediaItemProps> getMediaItemProperties(String itemPath) async {
    final payload = await _callAsync(
      BLUEZ_MEDIA_OP_ITEM_GET_PROPERTIES,
      objectPath: itemPath,
    );
    return GlazeCodec.decode<BlueZMediaItemProps>(payload!, 0);
  }

  Future<String> getItemCoverArt(
    String itemPath,
    String targetFile, {
    Duration timeout = const Duration(seconds: 15),
  }) {
    _validateCoverArtRequest(targetFile, timeout);
    return _callAsync(
      BLUEZ_MEDIA_OP_ITEM_GET_COVER_ART,
      objectPath: itemPath,
      argument: targetFile,
      value: timeout.inMilliseconds,
    ).then((_) => targetFile);
  }

  Future<String> getItemCoverArtFromExistingSession(
    String itemPath,
    String targetFile, {
    Duration timeout = const Duration(seconds: 15),
  }) {
    _validateCoverArtRequest(targetFile, timeout);
    return _callAsync(
      BLUEZ_MEDIA_OP_ITEM_GET_COVER_ART_FROM_EXISTING_SESSION,
      objectPath: itemPath,
      argument: targetFile,
      value: timeout.inMilliseconds,
    ).then((_) => targetFile);
  }

  // ── org.bluez.MediaTransport1 remote transports ────────────────────────────

  Future<BlueZMediaAcquireResult> transportAcquire(String transportPath) async {
    final payload = await _callAsync(
      BLUEZ_MEDIA_OP_TRANSPORT_ACQUIRE,
      objectPath: transportPath,
    );
    return GlazeCodec.decode<BlueZMediaAcquireResult>(payload!, 0);
  }

  Future<BlueZMediaAcquireResult> transportTryAcquire(
    String transportPath,
  ) async {
    final payload = await _callAsync(
      BLUEZ_MEDIA_OP_TRANSPORT_TRY_ACQUIRE,
      objectPath: transportPath,
    );
    return GlazeCodec.decode<BlueZMediaAcquireResult>(payload!, 0);
  }

  Future<void> transportRelease(String transportPath) =>
      _callAsync(BLUEZ_MEDIA_OP_TRANSPORT_RELEASE, objectPath: transportPath);

  Future<BlueZMediaTransportProps> getMediaTransportProperties(
    String transportPath,
  ) async {
    final payload = await _callAsync(
      BLUEZ_MEDIA_OP_TRANSPORT_GET_PROPERTIES,
      objectPath: transportPath,
    );
    return GlazeCodec.decode<BlueZMediaTransportProps>(payload!, 0);
  }

  Future<void> transportSetVolume(String transportPath, int volume) {
    if (volume < 0 || volume > 127) {
      throw RangeError.range(volume, 0, 127, 'volume');
    }
    return _callAsync(
      BLUEZ_MEDIA_OP_TRANSPORT_SET_VOLUME,
      objectPath: transportPath,
      value: volume,
    );
  }

  Future<BlueZMediaManagedObjects> getManagedObjects() async {
    final payload = await _callAsync(BLUEZ_MEDIA_OP_GET_MANAGED_OBJECTS);
    return GlazeCodec.decode<BlueZMediaManagedObjects>(payload!, 0);
  }

  void closeFileDescriptor(int fd) {
    final result = _bindings.bluez_media_close_fd(fd);
    _checkResult(result, 'close acquired media transport file descriptor');
  }

  void _onEvent(dynamic message) {
    if (message is! Uint8List || message.isEmpty || _closed) return;
    try {
      _dispatchEvent(message);
    } catch (error, stack) {
      // A malformed native event must not terminate the ReceivePort listener.
      // Catch Object (not just Exception) so Dart Errors from codec reads are
      // also logged rather than silently swallowed.
      // ignore: avoid_print
      print('[bluez_media] event decode failed: $error\n$stack');
    }
  }

  void _dispatchEvent(Uint8List message) {
    switch (message[0]) {
      case 0x30:
      case 0x31:
        final available = message[0] == 0x31;
        if (_serviceAvailable != available) {
          _serviceAvailable = available;
          _serviceAvailabilityCtrl.add(available);
        }
        return;
      case 0x00:
        if (!_ready.isCompleted) _ready.complete();
        return;
      case 0x01:
        final props = GlazeCodec.decode<BlueZMediaPlayerProps>(message, 1);
        if (_closed) return;
        final existing = _players[props.objectPath];
        final proxy = _player(props.objectPath)..updateProps(props);
        if (existing == null) _playerAddedCtrl.add(proxy);
      case 0x02:
        final props = GlazeCodec.decode<BlueZMediaControlProps>(message, 1);
        if (_closed) return;
        final existing = _controls[props.objectPath];
        final proxy = _control(props.objectPath)..updateProps(props);
        if (existing == null) _controlAddedCtrl.add(proxy);
      case 0x04:
        final props = GlazeCodec.decode<BlueZMediaTransportProps>(message, 1);
        if (_closed) return;
        final existing = _transports[props.objectPath];
        final proxy = _transport(props.objectPath);
        proxy.updateProps(props);
        if (existing == null) _transportAddedCtrl.add(proxy);
      case 0x05:
        final props = GlazeCodec.decode<BlueZMediaFolderProps>(message, 1);
        if (_closed) return;
        final existing = _folders[props.objectPath];
        final proxy = _folder(props.objectPath)..updateProps(props);
        if (existing == null) _folderAddedCtrl.add(proxy);
      case 0x06:
        final props = GlazeCodec.decode<BlueZMediaItemProps>(message, 1);
        if (_closed) return;
        final existing = _items[props.objectPath];
        final proxy = _item(props.objectPath)..updateProps(props);
        if (existing == null) _itemAddedCtrl.add(proxy);
      case 0x7E:
        if (_closed) return;
        _removeObject(GlazeCodec.decode<BlueZMediaObjectRemoved>(message, 1));
    }
  }

  void _removeObject(BlueZMediaObjectRemoved removed) {
    switch (removed.interfaceName) {
      case 'org.bluez.MediaPlayer1':
        final proxy = _players.remove(removed.objectPath);
        if (proxy != null) {
          _playerRemovedCtrl.add(proxy);
          proxy.dispose();
        }
      case 'org.bluez.MediaControl1':
        final proxy = _controls.remove(removed.objectPath);
        if (proxy != null) {
          _controlRemovedCtrl.add(proxy);
          proxy.dispose();
        }
      case 'org.bluez.MediaFolder1':
        final proxy = _folders.remove(removed.objectPath);
        if (proxy != null) {
          _folderRemovedCtrl.add(proxy);
          proxy.dispose();
        }
      case 'org.bluez.MediaItem1':
        final proxy = _items.remove(removed.objectPath);
        if (proxy != null) {
          _itemRemovedCtrl.add(proxy);
          proxy.dispose();
        }
      case 'org.bluez.MediaTransport1':
        final proxy = _transports.remove(removed.objectPath);
        if (proxy != null) {
          _transportRemovedCtrl.add(proxy);
          proxy.dispose();
        }
    }
  }

  // ── Error handling ─────────────────────────────────────────────────────────

  void _ensureOpen() {
    // Check _closed first: close() sets it before nulling _handle, so this
    // prevents calls from slipping through the window between the two.
    if (_closed || _handle == nullptr) {
      throw StateError('BluezMediaClient is closed.');
    }
  }

  Future<Uint8List?> _callAsync(
    int operation, {
    String? objectPath,
    String? argument,
    int value = 0,
  }) async {
    _ensureOpen();
    final resultPort = ReceivePort('bluez_media.operation');
    final objectPathPtr = objectPath?.toNativeUtf8();
    final argumentPtr = argument?.toNativeUtf8();
    try {
      _bindings.bluez_media_call_async(
        _handle,
        operation,
        objectPathPtr?.cast<Char>() ?? nullptr.cast<Char>(),
        argumentPtr?.cast<Char>() ?? nullptr.cast<Char>(),
        value,
        resultPort.sendPort.nativePort,
      );
      return await _awaitNativeResult(resultPort);
    } finally {
      if (argumentPtr != null) calloc.free(argumentPtr);
      if (objectPathPtr != null) calloc.free(objectPathPtr);
    }
  }

  static Future<Uint8List?> _awaitNativeResult(ReceivePort port) async {
    try {
      final result = await port.first;
      if (result is! Uint8List || result.isEmpty) {
        throw const BlueZMediaOperationException(
          'Native media operation returned an invalid result.',
          name: 'org.bluez.Error.Failed',
        );
      }
      switch (result[0]) {
        case 0xFF:
          return null;
        case 0x10:
          return Uint8List.sublistView(result, 1);
        case 0x20:
          throw _exceptionFromResult(result);
        default:
          throw const BlueZMediaOperationException(
            'Native media operation returned an unknown result.',
            name: 'org.bluez.Error.Failed',
          );
      }
    } finally {
      port.close();
    }
  }

  static BlueZMediaException _exceptionFromResult(
    Object? result, {
    bool serviceUnavailable = false,
  }) {
    if (result is Uint8List && result.isNotEmpty && result[0] == 0x20) {
      final error = GlazeCodec.decode<BlueZMediaError>(result, 1);
      if (serviceUnavailable) {
        return BlueZMediaServiceUnavailableException(error.message);
      }
      return BlueZMediaOperationException(
        error.message,
        name: error.name,
        objectPath: error.objectPath,
      );
    }
    return serviceUnavailable
        ? const BlueZMediaServiceUnavailableException()
        : const BlueZMediaOperationException(
            'Native media operation failed.',
            name: 'org.bluez.Error.Failed',
          );
  }

  static void _checkResult(int result, String operation) {
    if (result == 0) {
      return;
    }
    throw StateError('Failed to $operation: native error $result.');
  }

  static void _checkMediaPlayerMode(
    String value,
    Set<String> allowedValues,
    String property,
  ) {
    if (allowedValues.contains(value)) {
      return;
    }
    throw ArgumentError.value(
      value,
      property,
      'Expected one of: ${allowedValues.join(', ')}',
    );
  }

  static void _validateRegistrationConfig(
    BluezMediaPlayerRegistrationConfig config,
  ) {
    if (!config.adapterPath.startsWith('/') ||
        !config.playerPath.startsWith('/')) {
      throw ArgumentError(
        'Adapter and player paths must be D-Bus object paths.',
      );
    }
    if (!_localPlayerTypes.contains(config.type)) {
      throw ArgumentError.value(
        config.type,
        'type',
        'Unsupported player type.',
      );
    }
    if (config.subtype.isNotEmpty &&
        !_localPlayerSubtypes.contains(config.subtype)) {
      throw ArgumentError.value(
        config.subtype,
        'subtype',
        'Unsupported player subtype.',
      );
    }
    if (config.browsable || config.searchable) {
      throw UnsupportedError(
        'Local player browsing requires a MediaFolder1 implementation.',
      );
    }
  }
}

/// The dynamic library in which the symbols for [BluezMediaNativeBindings] can be found.
final DynamicLibrary _dylib = loadBluezMediaNative();

/// The bindings to the native functions in [_dylib].
final BluezMediaNativeBindings _bindings = BluezMediaNativeBindings(_dylib);
bool _nativeApiInitialized = false;

const _repeatModes = {'off', 'singletrack', 'alltracks', 'group'};
const _shuffleModes = {'off', 'alltracks', 'group'};
const _localPlayerTypes = {
  'Audio',
  'Video',
  'Audio Broadcasting',
  'Video Broadcasting',
};
const _localPlayerSubtypes = {'Audio Book', 'Podcast'};

void _initializeNativeApi() {
  if (_nativeApiInitialized) return;
  _bindings.bluez_media_init(NativeApi.initializeApiDLData);
  _nativeApiInitialized = true;
}
