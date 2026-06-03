import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import 'bluez_media_native_bindings_generated.dart';
import 'src/ffi/codec.dart';
import 'src/ffi/types.dart';

export 'src/ffi/types.dart' show BlueZMediaPlayerProps, BlueZMediaProperty;

class BluezMediaPlayerRegistrationConfig {
  final String adapterPath;
  final String playerPath;
  final String identity;
  final String name;
  final String type;
  final String subtype;
  final String status;
  final int positionMs;
  final bool canGoNext;
  final bool canGoPrevious;
  final bool canPlay;
  final bool canPause;
  final bool canSeek;
  final bool canControl;
  final bool browsable;
  final bool searchable;

  const BluezMediaPlayerRegistrationConfig({
    required this.adapterPath,
    required this.playerPath,
    this.identity = 'bluez_media_native',
    this.name = 'bluez_media_native',
    this.type = 'audio',
    this.subtype = '',
    this.status = 'stopped',
    this.positionMs = 0,
    this.canGoNext = false,
    this.canGoPrevious = false,
    this.canPlay = true,
    this.canPause = true,
    this.canSeek = false,
    this.canControl = true,
    this.browsable = false,
    this.searchable = false,
  });
}

class BluezMediaClient {
  Pointer<Void> _handle;

  BluezMediaClient._(this._handle);

  factory BluezMediaClient.create() {
    final handle = _bindings.bluez_media_client_create();
    if (handle == nullptr) {
      throw StateError('Unable to connect to BlueZ on the system bus.');
    }
    return BluezMediaClient._(handle);
  }

  void close() {
    if (_handle == nullptr) {
      return;
    }
    _bindings.bluez_media_client_destroy(_handle);
    _handle = nullptr;
  }

  void registerPlayer(BluezMediaPlayerRegistrationConfig config) {
    _ensureOpen();

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
        ..identity = nativeString(config.identity)
        ..name = nativeString(config.name)
        ..type = nativeString(config.type)
        ..subtype = nativeString(config.subtype)
        ..status = nativeString(config.status)
        ..position_ms = config.positionMs
        ..can_go_next = config.canGoNext ? 1 : 0
        ..can_go_previous = config.canGoPrevious ? 1 : 0
        ..can_play = config.canPlay ? 1 : 0
        ..can_pause = config.canPause ? 1 : 0
        ..can_seek = config.canSeek ? 1 : 0
        ..can_control = config.canControl ? 1 : 0
        ..browsable = config.browsable ? 1 : 0
        ..searchable = config.searchable ? 1 : 0;

      final result = _bindings.bluez_media_register_player(
        _handle,
        registration,
      );
      _checkResult(result, 'register player');
    } finally {
      for (final pointer in strings) {
        calloc.free(pointer);
      }
      calloc.free(registration);
    }
  }

  void unregisterPlayer({
    required String adapterPath,
    required String playerPath,
  }) {
    _ensureOpen();

    final adapterPathPtr = adapterPath.toNativeUtf8();
    final playerPathPtr = playerPath.toNativeUtf8();
    try {
      final result = _bindings.bluez_media_unregister_player(
        _handle,
        adapterPathPtr.cast<Char>(),
        playerPathPtr.cast<Char>(),
      );
      _checkResult(result, 'unregister player');
    } finally {
      calloc.free(adapterPathPtr);
      calloc.free(playerPathPtr);
    }
  }

  void play(String playerPath) {
    _callPlayerControl(playerPath, _bindings.bluez_media_player_play, 'play');
  }

  void pause(String playerPath) {
    _callPlayerControl(playerPath, _bindings.bluez_media_player_pause, 'pause');
  }

  void stop(String playerPath) {
    _callPlayerControl(playerPath, _bindings.bluez_media_player_stop, 'stop');
  }

  void next(String playerPath) {
    _callPlayerControl(playerPath, _bindings.bluez_media_player_next, 'next');
  }

  void previous(String playerPath) {
    _callPlayerControl(
      playerPath,
      _bindings.bluez_media_player_previous,
      'previous',
    );
  }

  BlueZMediaPlayerProps getPlayerProperties(String playerPath) {
    _ensureOpen();

    final playerPathPtr = playerPath.toNativeUtf8();
    try {
      final size = _bindings.bluez_media_player_get_properties(
        _handle,
        playerPathPtr.cast<Char>(),
        nullptr,
        0,
      );
      if (size < 0) {
        _checkResult(size, 'get player properties size');
      }

      final out = calloc<Uint8>(size);
      try {
        final result = _bindings.bluez_media_player_get_properties(
          _handle,
          playerPathPtr.cast<Char>(),
          out,
          size,
        );
        if (result < 0) {
          _checkResult(result, 'get player properties');
        }

        final bytes = Uint8List.fromList(out.asTypedList(result));
        return GlazeCodec.decode<BlueZMediaPlayerProps>(bytes, 0);
      } finally {
        calloc.free(out);
      }
    } finally {
      calloc.free(playerPathPtr);
    }
  }

  void _ensureOpen() {
    if (_handle == nullptr) {
      throw StateError('BluezMediaClient is closed.');
    }
  }

  void _callPlayerControl(
    String playerPath,
    int Function(Pointer<Void>, Pointer<Char>) call,
    String operation,
  ) {
    _ensureOpen();

    final playerPathPtr = playerPath.toNativeUtf8();
    try {
      final result = call(_handle, playerPathPtr.cast<Char>());
      _checkResult(result, '$operation player');
    } finally {
      calloc.free(playerPathPtr);
    }
  }

  static void _checkResult(int result, String operation) {
    if (result == 0) {
      return;
    }
    throw StateError('Failed to $operation: native error $result.');
  }
}

/// A very short-lived native function.
///
/// For very short-lived functions, it is fine to call them on the main isolate.
/// They will block the Dart execution while running the native function, so
/// only do this for native functions which are guaranteed to be short-lived.
int sum(int a, int b) => _bindings.sum(a, b);

/// A longer lived native function, which occupies the thread calling it.
///
/// Do not call these kind of native functions in the main isolate. They will
/// block Dart execution. This will cause dropped frames in Flutter applications.
/// Instead, call these native functions on a separate isolate.
///
/// Modify this to suit your own use case. Example use cases:
///
/// 1. Reuse a single isolate for various different kinds of requests.
/// 2. Use multiple helper isolates for parallel execution.
Future<int> sumAsync(int a, int b) async {
  final SendPort helperIsolateSendPort = await _helperIsolateSendPort;
  final int requestId = _nextSumRequestId++;
  final _SumRequest request = _SumRequest(requestId, a, b);
  final Completer<int> completer = Completer<int>();
  _sumRequests[requestId] = completer;
  helperIsolateSendPort.send(request);
  return completer.future;
}

const String _libName = 'bluez_media_native';

/// The dynamic library in which the symbols for [BluezMediaNativeBindings] can be found.
final DynamicLibrary _dylib = () {
  if (Platform.isMacOS || Platform.isIOS) {
    return DynamicLibrary.open('$_libName.framework/$_libName');
  }
  if (Platform.isAndroid || Platform.isLinux) {
    return DynamicLibrary.open('lib$_libName.so');
  }
  if (Platform.isWindows) {
    return DynamicLibrary.open('$_libName.dll');
  }
  throw UnsupportedError('Unknown platform: ${Platform.operatingSystem}');
}();

/// The bindings to the native functions in [_dylib].
final BluezMediaNativeBindings _bindings = BluezMediaNativeBindings(_dylib);

/// A request to compute `sum`.
///
/// Typically sent from one isolate to another.
class _SumRequest {
  final int id;
  final int a;
  final int b;

  const _SumRequest(this.id, this.a, this.b);
}

/// A response with the result of `sum`.
///
/// Typically sent from one isolate to another.
class _SumResponse {
  final int id;
  final int result;

  const _SumResponse(this.id, this.result);
}

/// Counter to identify [_SumRequest]s and [_SumResponse]s.
int _nextSumRequestId = 0;

/// Mapping from [_SumRequest] `id`s to the completers corresponding to the correct future of the pending request.
final Map<int, Completer<int>> _sumRequests = <int, Completer<int>>{};

/// The SendPort belonging to the helper isolate.
Future<SendPort> _helperIsolateSendPort = () async {
  // The helper isolate is going to send us back a SendPort, which we want to
  // wait for.
  final Completer<SendPort> completer = Completer<SendPort>();

  // Receive port on the main isolate to receive messages from the helper.
  // We receive two types of messages:
  // 1. A port to send messages on.
  // 2. Responses to requests we sent.
  final ReceivePort receivePort = ReceivePort()
    ..listen((dynamic data) {
      if (data is SendPort) {
        // The helper isolate sent us the port on which we can sent it requests.
        completer.complete(data);
        return;
      }
      if (data is _SumResponse) {
        // The helper isolate sent us a response to a request we sent.
        final Completer<int> completer = _sumRequests[data.id]!;
        _sumRequests.remove(data.id);
        completer.complete(data.result);
        return;
      }
      throw UnsupportedError('Unsupported message type: ${data.runtimeType}');
    });

  // Start the helper isolate.
  await Isolate.spawn((SendPort sendPort) async {
    final ReceivePort helperReceivePort = ReceivePort()
      ..listen((dynamic data) {
        // On the helper isolate listen to requests and respond to them.
        if (data is _SumRequest) {
          final int result = _bindings.sum_long_running(data.a, data.b);
          final _SumResponse response = _SumResponse(data.id, result);
          sendPort.send(response);
          return;
        }
        throw UnsupportedError('Unsupported message type: ${data.runtimeType}');
      });

    // Send the port to the main isolate on which we can receive requests.
    sendPort.send(helperReceivePort.sendPort);
  }, receivePort.sendPort);

  // Wait until the helper isolate has sent us back the SendPort on which we
  // can start sending requests.
  return completer.future;
}();
