import 'dart:io';
import 'dart:isolate';
import 'package:test/test.dart';

void main() {
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

int socketCount() => Directory('/proc/self/fd').listSync().where((entry) {
  try {
    return Link(entry.path).targetSync().startsWith('socket:');
  } on FileSystemException {
    return false;
  }
}).length;
