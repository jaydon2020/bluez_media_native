import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:bluez_media_native/bluez_media_native.dart';
import 'package:bluez_media_native/bluez_media_native_bindings_generated.dart';
import 'package:bluez_media_native/src/ffi/codec.dart';
import 'package:bluez_media_native/src/internal/library_loader.dart';
import 'package:ffi/ffi.dart';
import 'package:test/test.dart';

void main() {
  setUp(() async {
    if (Platform.environment['BLUEZ_TEST_BUS'] == '1') await step('reset');
  });
  test(
    'connection failures preserve the original D-Bus error details',
    () async {
      await step('fail_snapshot');
      await expectLater(
        BluezMediaClient.create(),
        throwsA(
          isA<BlueZMediaServiceUnavailableException>()
              .having(
                (e) => e.name,
                'name',
                'org.freedesktop.DBus.Error.AccessDenied',
              )
              .having((e) => e.message, 'message', 'Snapshot denied'),
        ),
      );
    },
    skip: Platform.environment['BLUEZ_TEST_BUS'] != '1',
  );
  test('close completes only after queued native operations finish', () async {
    final client = await BluezMediaClient.create();
    var operationFinished = false;
    final operation = client
        .player('/player')
        .play()
        .then((_) => operationFinished = true);
    final closing = client.close();
    expect(identical(closing, client.close()), isTrue);
    await closing;
    expect(operationFinished, isTrue);
    await operation;
  }, skip: Platform.environment['BLUEZ_TEST_BUS'] != '1');
  test('claimed fd is reclaimed when its isolate group exits', () async {
    final ready = ReceivePort();
    final exited = ReceivePort();
    final isolate = await Isolate.spawnUri(
      File('test/support/client_isolate.dart').absolute.uri,
      ['acquire'],
      ready.sendPort,
      onExit: exited.sendPort,
    );
    final fd = await ready.first.timeout(const Duration(seconds: 10)) as int;
    ready.close();
    final descriptor = Link('/proc/self/fd/$fd');
    expect(descriptor.existsSync(), isTrue);
    isolate.kill(priority: Isolate.immediate);
    await exited.first.timeout(const Duration(seconds: 10));
    exited.close();
    for (var i = 0; i < 40 && descriptor.existsSync(); i++) {
      await Future<void>.delayed(const Duration(milliseconds: 25));
    }
    expect(descriptor.existsSync(), isFalse);
  }, skip: Platform.environment['BLUEZ_TEST_BUS'] != '1');
  test(
    'acquired transport owns a real fd and closes it exactly once',
    () async {
      final client = await BluezMediaClient.create();
      try {
        final transport = await client.transport('/transport').acquire();
        final descriptor = Link('/proc/self/fd/${transport.fd}');
        expect(descriptor.existsSync(), isTrue);
        expect(transport.readMtu, 672);
        transport.close();
        expect(transport.isClosed, isTrue);
        expect(descriptor.existsSync(), isFalse);
        transport.close();
      } finally {
        await client.close();
      }
    },
    skip: Platform.environment['BLUEZ_TEST_BUS'] != '1',
  );

  test('unclaimed acquisition is reclaimed with its client', () async {
    final bindings = BluezMediaNativeBindings(loadBluezMediaNative());
    bindings.bluez_media_init(NativeApi.initializeApiDLData);
    final handle = bindings.bluez_media_client_create(0);
    final port = ReceivePort();
    final path = '/transport'.toNativeUtf8();
    try {
      bindings.bluez_media_call_async(
        handle,
        BLUEZ_MEDIA_OP_TRANSPORT_ACQUIRE,
        path.cast(),
        nullptr,
        0,
        port.sendPort.nativePort,
      );
      final bytes = await port.first as List<int>;
      expect(bytes.first, 0x11);
      final data = Uint8List.fromList(bytes);
      final result = GlazeCodec.decode<BlueZMediaAcquireResult>(data, 9);
      final descriptor = Link('/proc/self/fd/${result.fd}');
      expect(descriptor.existsSync(), isTrue);
      bindings.bluez_media_client_destroy(handle);
      for (var i = 0; i < 40 && descriptor.existsSync(); i++) {
        await Future<void>.delayed(const Duration(milliseconds: 25));
      }
      expect(descriptor.existsSync(), isFalse);
    } finally {
      port.close();
      calloc.free(path);
      bindings.bluez_media_client_destroy(handle);
    }
  }, skip: Platform.environment['BLUEZ_TEST_BUS'] != '1');
  test(
    'cover-art timeout cancels the transfer and deletes its partial file',
    () async {
      final client = await BluezMediaClient.create();
      final directory = await Directory.systemTemp.createTemp(
        'bluez-cover-test-',
      );
      final target = File('${directory.path}/art');
      try {
        final download = expectLater(
          client.getPlayerCoverArt(
            '/player',
            target.path,
            timeout: const Duration(milliseconds: 450),
          ),
          throwsA(isA<BlueZMediaException>()),
        );
        await Future<void>.delayed(const Duration(milliseconds: 50));
        // Cover-art waits must not occupy the normal command worker.
        await client
            .getPlayerProperties('/player')
            .timeout(const Duration(milliseconds: 250));
        await download;
        await step('assert_cancelled');
        expect(target.existsSync(), isFalse);
      } finally {
        await client.close();
        await directory.delete(recursive: true);
      }
    },
    skip: Platform.environment['BLUEZ_TEST_BUS'] != '1',
  );
  test('slow method replies do not block property events', () async {
    final client = await BluezMediaClient.create();
    try {
      final player = client.players.single;
      await player.refresh();
      final changed = player.propertiesChanged.firstWhere(
        (_) => player.status == 'stopped',
      );
      var completed = false;
      final call = player.play().then((_) {
        completed = true;
      });
      await changed.timeout(const Duration(milliseconds: 500));
      expect(completed, isFalse);
      await call;
    } finally {
      await client.close();
    }
  }, skip: Platform.environment['BLUEZ_TEST_BUS'] != '1');
  test(
    'resync replays property changes received before its snapshot reply',
    () async {
      final client = await BluezMediaClient.create();
      try {
        final available = client.serviceAvailabilityChanged.firstWhere(
          (v) => v,
        );
        await step('restart_present');
        await available.timeout(const Duration(seconds: 2));
        expect(client.players.single.status, 'playing');
      } finally {
        await client.close();
      }
    },
    skip: Platform.environment['BLUEZ_TEST_BUS'] != '1',
  );
  test('owner replacement removes stale proxies and resynchronizes', () async {
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
  }, skip: Platform.environment['BLUEZ_TEST_BUS'] != '1');
  test(
    'invalidation still resolves when another property changes during GetAll',
    () async {
      final client = await BluezMediaClient.create();
      try {
        final player = client.players.single;
        await player.refresh();
        final updated = player.propertiesChanged.firstWhere(
          (_) => player.status == 'stopped',
        );
        await step('invalidate_race');
        await updated.timeout(const Duration(seconds: 2));
        expect(player.status, 'stopped');
      } finally {
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
  test('property changes during discovery reach the initial proxy', () async {
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
  }, skip: Platform.environment['BLUEZ_TEST_BUS'] != '1');
  test('raw registration calls serialize across isolates', () async {
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
                final adapter = '/'.toNativeUtf8(allocator: arena).cast<Char>();
                final path = '/local_${worker}_$i'
                    .toNativeUtf8(allocator: arena)
                    .cast<Char>();
                registration.ref
                  ..adapter_path = adapter
                  ..player_path = path;
                final regRes = api.bluez_media_register_player(
                  token,
                  registration,
                );
                final unregRes = api.bluez_media_unregister_player(
                  token,
                  adapter,
                  path,
                );
                if (regRes != 0 || unregRes != 0) {
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
  }, skip: Platform.environment['BLUEZ_TEST_BUS'] != '1');
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
