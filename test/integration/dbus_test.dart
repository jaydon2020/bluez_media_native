import 'dart:ffi';
import 'dart:io';
import 'package:ffi/ffi.dart';
import 'package:bluez_media_native/bluez_media_native_bindings_generated.dart';
import 'package:bluez_media_native/src/internal/library_loader.dart';
import 'dart:isolate';
import 'package:test/test.dart';

void main() {
  test(
    'raw registration calls serialize across isolates',
    () async {
      final bindings = BluezMediaNativeBindings(loadBluezMediaNative());
      bindings.bluez_media_init(NativeApi.initializeApiDLData);
      final handle = bindings.bluez_media_client_create(0);
      expect(handle, isNot(nullptr));
      final address = handle.address;
      try {
        await Future.wait(
          List.generate(
            3,
            (worker) => Isolate.run(() {
              final api = BluezMediaNativeBindings(loadBluezMediaNative());
              final token = Pointer<Void>.fromAddress(address);
              for (var i = 0; i < 10; i++) {
                using((arena) {
                  final registration = arena<BluezMediaPlayerRegistration>();
                  final adapter = '/'
                      .toNativeUtf8(allocator: arena)
                      .cast<Char>();
                  final path = '/local_${worker}_$i'
                      .toNativeUtf8(allocator: arena)
                      .cast<Char>();
                  registration.ref
                    ..adapter_path = adapter
                    ..player_path = path;
                  if (api.bluez_media_register_player(token, registration) !=
                          0 ||
                      api.bluez_media_unregister_player(token, adapter, path) !=
                          0) {
                    throw StateError('Registration failed');
                  }
                });
              }
            }),
          ),
        );
      } finally {
        bindings.bluez_media_client_destroy(handle);
      }
    },
    skip: Platform.environment['BLUEZ_TEST_BUS'] != '1',
  );
  test(
    'client resources are retired when their isolate group shuts down',
    () async {
      final before = socketCount();
      for (var i = 0; i < 3; i++) {
        final ready = ReceivePort();
        final exited = ReceivePort();
        final isolate = await Isolate.spawnUri(
          File('test/support/client_isolate.dart').absolute.uri,
          [],
          ready.sendPort,
          onExit: exited.sendPort,
        );
        await ready.first.timeout(const Duration(seconds: 10));
        ready.close();
        isolate.kill(priority: Isolate.immediate);
        await exited.first.timeout(const Duration(seconds: 10));
        exited.close();
      }
      for (var i = 0; i < 40 && socketCount() > before; i++) {
        await Future<void>.delayed(const Duration(milliseconds: 50));
      }
      expect(socketCount(), lessThanOrEqualTo(before));
      // The process must also exit cleanly after cached native variants existed.
    },
    skip: Platform.environment['BLUEZ_TEST_BUS'] != '1',
  );
}

int socketCount() => Directory('/proc/self/fd').listSync(followLinks: false).where((entry) {
  try {
    return Link(entry.path).targetSync().startsWith('socket:');
  } on FileSystemException {
    return false;
  }
}).length;
