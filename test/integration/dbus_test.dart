import 'dart:ffi';
import 'package:bluez_media_native/bluez_media_native.dart';
import 'dart:io';
import 'package:ffi/ffi.dart';
import 'package:bluez_media_native/bluez_media_native_bindings_generated.dart';
import 'package:bluez_media_native/src/internal/library_loader.dart';
import 'dart:isolate';
import 'package:test/test.dart';

void main() {
  setUp(() async {
    if (Platform.environment['BLUEZ_TEST_BUS'] == '1') await step('reset');
  });
  test(
    'owner replacement removes stale proxies and resynchronizes',
    () async {
      final client = await BluezMediaClient.create();
      const registration = BluezMediaPlayerRegistrationConfig(
        adapterPath: '/',
        playerPath: '/local',
      );
      await client.registerPlayer(registration);
      final old = client.players.single;
      final availability = <bool>[];
      final sub = client.serviceAvailabilityChanged.listen(availability.add);
      try {
        await step('restart');
        for (var i = 0; i < 40 && !availability.contains(true); i++) {
          await Future<void>.delayed(const Duration(milliseconds: 25));
        }
        expect(availability, [false, true]);
        expect(client.players, isEmpty);
        expect(old.isDisposed, isTrue);
        expect(client.isServiceAvailable, isTrue);
        await client.registerPlayer(registration);
      } finally {
        await sub.cancel();
        await client.close();
      }
    },
    skip: Platform.environment['BLUEZ_TEST_BUS'] != '1',
  );
  test(
    'invalidation fetches a replacement without fabricating defaults',
    () async {
      final client = await BluezMediaClient.create();
      try {
        final player = client.players.single;
        await player.refresh();
        final seen = <String>[];
        final sub = player.propertiesChanged.listen(
          (_) => seen.add(player.status),
        );
        await step('invalidate');
        await Future<void>.delayed(const Duration(milliseconds: 150));
        expect(player.status, 'playing');
        expect(seen, isNot(contains('')));
        await sub.cancel();
      } finally {
        await client.close();
      }
    },
    skip: Platform.environment['BLUEZ_TEST_BUS'] != '1',
  );
  test(
    'property changes during discovery reach the initial proxy',
    () async {
      final client = await BluezMediaClient.create();
      try {
        for (
          var i = 0;
          i < 40 && client.players.single.status != 'playing';
          i++
        ) {
          await Future<void>.delayed(const Duration(milliseconds: 25));
        }
        expect(client.players.single.status, 'playing');
      } finally {
        await client.close();
      }
    },
    skip: Platform.environment['BLUEZ_TEST_BUS'] != '1',
  );
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

int socketCount() =>
    Directory('/proc/self/fd').listSync(followLinks: false).where((entry) {
      try {
        return Link(entry.path).targetSync().startsWith('socket:');
      } on FileSystemException {
        return false;
      }
    }).length;

Future<void> step(String command) async {
  final result = await Process.run('dbus-send', [
    '--session',
    '--print-reply',
    '--dest=org.bluez',
    '/',
    'review.Test.Step',
    'string:$command',
  ]);
  if (result.exitCode != 0) throw StateError('${result.stderr}');
}
