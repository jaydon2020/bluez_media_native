// SPDX-License-Identifier: Apache-2.0
//
// proxy_corner_cases_test.dart — corner cases for the Dart proxy wrappers
// (BluezMediaPlayer, BluezMediaControl, BluezMediaFolder, BluezMediaItem,
//  BluezMediaTransport, BluezMediaAcquiredTransport).
//
// These tests are intentionally pure-Dart and use a FakeClient so they do
// not require a real BlueZ service.

import 'dart:async';

import 'package:bluez_media_native/bluez_media_native.dart';
import 'package:test/test.dart';

// ── Fake client ──────────────────────────────────────────────────────────────

class _FakeClient implements BluezMediaClient {
  final _closedFds = <int>[];
  var closeCallCount = 0;

  @override
  Future<BlueZMediaAcquireResult> transportAcquire(String path) async =>
      BlueZMediaAcquireResult(
        transportPath: path,
        fd: 123,
        readMtu: 1,
        writeMtu: 1,
      );

  @override
  void closeFileDescriptor(int fd) {
    closeCallCount++;
    _closedFds.add(fd);
  }

  @override
  dynamic noSuchMethod(Invocation invocation) => super.noSuchMethod(invocation);
}

// ── BluezMediaPlayer corner cases ────────────────────────────────────────────

void main() {
  group('BluezMediaPlayer.updateProps', () {
    late _FakeClient client;

    setUp(() => client = _FakeClient());

    test('no event emitted when props are identical', () async {
      final player = BluezMediaPlayer.internal(client, '/player');
      const props = BlueZMediaPlayerProps(
        objectPath: '/player',
        status: 'playing',
        position: 100,
      );
      player.updateProps(props);

      // A second call with the same values must not fire propertiesChanged.
      final events = <List<String>>[];
      final sub = player.propertiesChanged.listen(events.add);
      player.updateProps(props); // identical
      // Yield to the event loop; no event should arrive.
      await Future<void>.delayed(Duration.zero);
      await sub.cancel();

      expect(events, isEmpty);
      player.dispose();
    });

    test('only changed fields are reported', () async {
      final player = BluezMediaPlayer.internal(client, '/player');
      player.updateProps(
        const BlueZMediaPlayerProps(
          objectPath: '/player',
          status: 'playing',
          repeat: 'off',
          shuffle: 'off',
        ),
      );

      final changed = <List<String>>[];
      final sub = player.propertiesChanged.listen(changed.add);

      // Only status and position change.
      player.updateProps(
        const BlueZMediaPlayerProps(
          objectPath: '/player',
          status: 'paused', // changed
          repeat: 'off', // unchanged
          shuffle: 'off', // unchanged
          position: 5000, // changed
        ),
      );

      await Future<void>.delayed(Duration.zero);
      await sub.cancel();

      expect(changed.length, 1);
      expect(changed.single, containsAll(['Status', 'Position']));
      expect(changed.single, isNot(contains('Repeat')));
      expect(changed.single, isNot(contains('Shuffle')));
      player.dispose();
    });

    test('track property comparison is by value not reference', () async {
      final player = BluezMediaPlayer.internal(client, '/player');
      const props1 = BlueZMediaPlayerProps(
        objectPath: '/player',
        track: [BlueZMediaProperty(key: 'Title', value: 'Song A')],
      );
      player.updateProps(props1);

      final changed = <List<String>>[];
      final sub = player.propertiesChanged.listen(changed.add);

      // Same content, different list instance → no change.
      player.updateProps(
        const BlueZMediaPlayerProps(
          objectPath: '/player',
          track: [BlueZMediaProperty(key: 'Title', value: 'Song A')],
        ),
      );
      await Future<void>.delayed(Duration.zero);
      expect(changed, isEmpty);

      // Different value → change reported.
      player.updateProps(
        const BlueZMediaPlayerProps(
          objectPath: '/player',
          track: [BlueZMediaProperty(key: 'Title', value: 'Song B')],
        ),
      );
      await Future<void>.delayed(Duration.zero);
      await sub.cancel();

      expect(changed.length, 1);
      expect(changed.single, contains('Track'));
      player.dispose();
    });

    test(
      'track length change is detected even when key/value match prefix',
      () async {
        final player = BluezMediaPlayer.internal(client, '/player');
        player.updateProps(
          const BlueZMediaPlayerProps(
            objectPath: '/player',
            track: [BlueZMediaProperty(key: 'Title', value: 'Song')],
          ),
        );

        final changed = <List<String>>[];
        final sub = player.propertiesChanged.listen(changed.add);

        // Add a second property — same first entry, but length differs.
        player.updateProps(
          const BlueZMediaPlayerProps(
            objectPath: '/player',
            track: [
              BlueZMediaProperty(key: 'Title', value: 'Song'),
              BlueZMediaProperty(key: 'Artist', value: 'Band'),
            ],
          ),
        );

        await Future<void>.delayed(Duration.zero);
        await sub.cancel();

        expect(changed.single, contains('Track'));
        player.dispose();
      },
    );

    test('imageHandle returns empty string when ImgHandle is absent', () {
      final player = BluezMediaPlayer.internal(client, '/player');
      player.updateProps(
        const BlueZMediaPlayerProps(
          objectPath: '/player',
          track: [
            BlueZMediaProperty(key: 'Title', value: 'No Cover'),
            BlueZMediaProperty(key: 'Artist', value: 'Unknown'),
          ],
        ),
      );

      expect(player.imageHandle, '');
      player.dispose();
    });

    test(
      'imageHandle returns value even when ImgHandle is not the first entry',
      () {
        final player = BluezMediaPlayer.internal(client, '/player');
        player.updateProps(
          const BlueZMediaPlayerProps(
            objectPath: '/player',
            track: [
              BlueZMediaProperty(key: 'Title', value: 'Song'),
              BlueZMediaProperty(key: 'ImgHandle', value: 'handle-007'),
              BlueZMediaProperty(key: 'Duration', value: '240000'),
            ],
          ),
        );

        expect(player.imageHandle, 'handle-007');
        player.dispose();
      },
    );

    test('default props have safe zero/empty values before first update', () {
      final player = BluezMediaPlayer.internal(client, '/player');

      expect(player.status, '');
      expect(player.position, 0);
      expect(player.track, isEmpty);
      expect(player.browsable, false);
      expect(player.imageHandle, '');
      player.dispose();
    });

    test('track list exposed as unmodifiable', () {
      final player = BluezMediaPlayer.internal(client, '/player');
      player.updateProps(
        const BlueZMediaPlayerProps(
          objectPath: '/player',
          track: [BlueZMediaProperty(key: 'Title', value: 'X')],
        ),
      );

      expect(
        () => player.track.add(const BlueZMediaProperty(key: 'k', value: 'v')),
        throwsUnsupportedError,
      );
      player.dispose();
    });

    test('dispose closes the propertiesChanged stream', () async {
      final player = BluezMediaPlayer.internal(client, '/player');
      final done = Completer<void>();
      player.propertiesChanged.listen(null, onDone: done.complete);

      player.dispose();

      // Stream should be closed after dispose.
      await done.future.timeout(const Duration(seconds: 1));
    });

    test('propertiesChanged stream is broadcast (multi-subscriber)', () async {
      final player = BluezMediaPlayer.internal(client, '/player');

      final sub1 = player.propertiesChanged.listen((_) {});
      final sub2 = player.propertiesChanged.listen((_) {});
      // A broadcast stream must not throw on second listen.
      expect(sub1, isNotNull);
      expect(sub2, isNotNull);

      await sub1.cancel();
      await sub2.cancel();
      player.dispose();
    });
  });

  // ── BluezMediaControl corner cases ──────────────────────────────────────────

  group('BluezMediaControl.updateProps', () {
    late _FakeClient client;

    setUp(() => client = _FakeClient());

    test('no event when props are identical', () async {
      final control = BluezMediaControl.internal(client, '/control');
      const props = BlueZMediaControlProps(
        objectPath: '/control',
        connected: true,
        player: '/player',
      );
      control.updateProps(props);

      final events = <List<String>>[];
      final sub = control.propertiesChanged.listen(events.add);
      control.updateProps(props); // same
      await Future<void>.delayed(Duration.zero);
      await sub.cancel();

      expect(events, isEmpty);
      control.dispose();
    });

    test('reports Connected change', () async {
      final control = BluezMediaControl.internal(client, '/control');
      control.updateProps(
        const BlueZMediaControlProps(objectPath: '/control', connected: true),
      );

      final changed = <List<String>>[];
      final sub = control.propertiesChanged.listen(changed.add);
      control.updateProps(const BlueZMediaControlProps(objectPath: '/control'));
      await Future<void>.delayed(Duration.zero);
      await sub.cancel();

      expect(changed.single, contains('Connected'));
      expect(changed.single, isNot(contains('Player')));
      control.dispose();
    });

    test('reports Player change without Connected change', () async {
      final control = BluezMediaControl.internal(client, '/control');
      control.updateProps(
        const BlueZMediaControlProps(
          objectPath: '/control',
          connected: true,
          player: '/player0',
        ),
      );

      final changed = <List<String>>[];
      final sub = control.propertiesChanged.listen(changed.add);
      control.updateProps(
        const BlueZMediaControlProps(
          objectPath: '/control',
          connected: true, // unchanged
          player: '/player1', // changed
        ),
      );
      await Future<void>.delayed(Duration.zero);
      await sub.cancel();

      expect(changed.single, contains('Player'));
      expect(changed.single, isNot(contains('Connected')));
      control.dispose();
    });

    test('default props are safe before first update', () {
      final control = BluezMediaControl.internal(client, '/control');

      expect(control.connected, false);
      expect(control.playerPath, '');
      control.dispose();
    });
  });

  // ── BluezMediaFolder corner cases ────────────────────────────────────────────

  group('BluezMediaFolder.updateProps', () {
    late _FakeClient client;

    setUp(() => client = _FakeClient());

    test('no event when props are identical', () async {
      final folder = BluezMediaFolder.internal(client, '/folder');
      const props = BlueZMediaFolderProps(
        objectPath: '/folder',
        numberOfItems: 10,
        name: 'Albums',
      );
      folder.updateProps(props);

      final events = <List<String>>[];
      final sub = folder.propertiesChanged.listen(events.add);
      folder.updateProps(props);
      await Future<void>.delayed(Duration.zero);
      await sub.cancel();

      expect(events, isEmpty);
      folder.dispose();
    });

    test('reports NumberOfItems change', () async {
      final folder = BluezMediaFolder.internal(client, '/folder');
      folder.updateProps(
        const BlueZMediaFolderProps(objectPath: '/folder', numberOfItems: 10),
      );

      final changed = <List<String>>[];
      final sub = folder.propertiesChanged.listen(changed.add);
      folder.updateProps(
        const BlueZMediaFolderProps(objectPath: '/folder', numberOfItems: 11),
      );
      await Future<void>.delayed(Duration.zero);
      await sub.cancel();

      expect(changed.single, contains('NumberOfItems'));
      folder.dispose();
    });

    test('reports Name change only', () async {
      final folder = BluezMediaFolder.internal(client, '/folder');
      folder.updateProps(
        const BlueZMediaFolderProps(
          objectPath: '/folder',
          numberOfItems: 5,
          name: 'Albums',
        ),
      );

      final changed = <List<String>>[];
      final sub = folder.propertiesChanged.listen(changed.add);
      folder.updateProps(
        const BlueZMediaFolderProps(
          objectPath: '/folder',
          numberOfItems: 5, // unchanged
          name: 'Artists', // changed
        ),
      );
      await Future<void>.delayed(Duration.zero);
      await sub.cancel();

      expect(changed.single, contains('Name'));
      expect(changed.single, isNot(contains('NumberOfItems')));
      folder.dispose();
    });

    test('numberOfItems = 0 is safe (no items folder)', () {
      final folder = BluezMediaFolder.internal(client, '/folder');
      folder.updateProps(const BlueZMediaFolderProps(objectPath: '/folder'));

      expect(folder.numberOfItems, 0);
      folder.dispose();
    });
  });

  // ── BluezMediaItem corner cases ──────────────────────────────────────────────

  group('BluezMediaItem.updateProps', () {
    late _FakeClient client;

    setUp(() => client = _FakeClient());

    test('no event when props are identical', () async {
      final item = BluezMediaItem.internal(client, '/item');
      const props = BlueZMediaItemProps(
        objectPath: '/item',
        name: 'Blue Train',
        playable: true,
      );
      item.updateProps(props);

      final events = <List<String>>[];
      final sub = item.propertiesChanged.listen(events.add);
      item.updateProps(props);
      await Future<void>.delayed(Duration.zero);
      await sub.cancel();

      expect(events, isEmpty);
      item.dispose();
    });

    test('metadata comparison detects key change', () async {
      final item = BluezMediaItem.internal(client, '/item');
      item.updateProps(
        const BlueZMediaItemProps(
          objectPath: '/item',
          metadata: [BlueZMediaProperty(key: 'OldKey', value: 'Value')],
        ),
      );

      final changed = <List<String>>[];
      final sub = item.propertiesChanged.listen(changed.add);
      item.updateProps(
        const BlueZMediaItemProps(
          objectPath: '/item',
          metadata: [BlueZMediaProperty(key: 'NewKey', value: 'Value')],
        ),
      );
      await Future<void>.delayed(Duration.zero);
      await sub.cancel();

      expect(changed.single, contains('Metadata'));
      item.dispose();
    });

    test('empty metadata list is equal to empty metadata list', () async {
      final item = BluezMediaItem.internal(client, '/item');
      const props = BlueZMediaItemProps(objectPath: '/item');
      item.updateProps(props);

      final events = <List<String>>[];
      final sub = item.propertiesChanged.listen(events.add);
      item.updateProps(props);
      await Future<void>.delayed(Duration.zero);
      await sub.cancel();

      expect(events, isEmpty);
      item.dispose();
    });

    test('reports Playable change only', () async {
      final item = BluezMediaItem.internal(client, '/item');
      item.updateProps(const BlueZMediaItemProps(objectPath: '/item'));

      final changed = <List<String>>[];
      final sub = item.propertiesChanged.listen(changed.add);
      item.updateProps(
        const BlueZMediaItemProps(objectPath: '/item', playable: true),
      );
      await Future<void>.delayed(Duration.zero);
      await sub.cancel();

      expect(changed.single, contains('Playable'));
      item.dispose();
    });
  });

  // ── BluezMediaTransport corner cases ─────────────────────────────────────────

  group('BluezMediaTransport.updateProps', () {
    late _FakeClient client;

    setUp(() => client = _FakeClient());

    test('no event when props are identical', () async {
      final transport = BluezMediaTransport.internal(client, '/transport');
      const props = BlueZMediaTransportProps(
        objectPath: '/transport',
        state: 'idle',
        volume: 50,
        configuration: [1, 2, 3],
      );
      transport.updateProps(props);

      final events = <List<String>>[];
      final sub = transport.propertiesChanged.listen(events.add);
      transport.updateProps(props);
      await Future<void>.delayed(Duration.zero);
      await sub.cancel();

      expect(events, isEmpty);
      transport.dispose();
    });

    test(
      'configuration: same length different values is detected as a change',
      () async {
        final transport = BluezMediaTransport.internal(client, '/transport');
        transport.updateProps(
          const BlueZMediaTransportProps(
            objectPath: '/transport',
            configuration: [1, 2],
          ),
        );

        final changed = <List<String>>[];
        final sub = transport.propertiesChanged.listen(changed.add);
        transport.updateProps(
          const BlueZMediaTransportProps(
            objectPath: '/transport',
            configuration: [1, 9], // same length, value[1] changed
          ),
        );
        await Future<void>.delayed(Duration.zero);
        await sub.cancel();

        expect(changed.single, contains('Configuration'));
        transport.dispose();
      },
    );

    test('configuration: same values in same order is NOT a change', () async {
      final transport = BluezMediaTransport.internal(client, '/transport');
      transport.updateProps(
        const BlueZMediaTransportProps(
          objectPath: '/transport',
          configuration: [0xFF, 0x00, 0x21],
        ),
      );

      final events = <List<String>>[];
      final sub = transport.propertiesChanged.listen(events.add);
      transport.updateProps(
        const BlueZMediaTransportProps(
          objectPath: '/transport',
          configuration: [0xFF, 0x00, 0x21], // identical
        ),
      );
      await Future<void>.delayed(Duration.zero);
      await sub.cancel();

      expect(events, isEmpty);
      transport.dispose();
    });

    test('empty configuration stays equal', () async {
      final transport = BluezMediaTransport.internal(client, '/transport');
      transport.updateProps(
        const BlueZMediaTransportProps(objectPath: '/transport'),
      );

      final events = <List<String>>[];
      final sub = transport.propertiesChanged.listen(events.add);
      transport.updateProps(
        const BlueZMediaTransportProps(objectPath: '/transport'),
      );
      await Future<void>.delayed(Duration.zero);
      await sub.cancel();

      expect(events, isEmpty);
      transport.dispose();
    });

    test('default props are safe before first update', () {
      final transport = BluezMediaTransport.internal(client, '/transport');

      expect(transport.state, '');
      expect(transport.volume, 0);
      expect(transport.configuration, isEmpty);
      expect(transport.codec, 0);
      transport.dispose();
    });
  });

  // ── BluezMediaAcquiredTransport (fd lifecycle) ───────────────────────────────

  group('BluezMediaAcquiredTransport fd lifecycle', () {
    test(
      'close() is idempotent – FakeClient fd close called exactly once',
      () async {
        final client = _FakeClient();
        final proxy = BluezMediaTransport.internal(client, '/transport');
        final acquired = await proxy.acquire();
        acquired.close();
        acquired.close();
        expect(acquired.isClosed, isTrue);
        expect(client.closeCallCount, 1);
        expect(client._closedFds, [123]);
        proxy.dispose();
      },
    );
  });

  // ── Proxy stream correctness ──────────────────────────────────────────────

  group('Proxy propertiesChanged streams are broadcast', () {
    late _FakeClient client;

    setUp(() => client = _FakeClient());

    test('BluezMediaControl stream allows multiple listeners', () {
      final control = BluezMediaControl.internal(client, '/ctrl');
      final s1 = control.propertiesChanged.listen((_) {});
      final s2 = control.propertiesChanged.listen((_) {}); // must not throw
      expect(s2, isNotNull);
      s1.cancel();
      s2.cancel();
      control.dispose();
    });

    test('BluezMediaFolder stream allows multiple listeners', () {
      final folder = BluezMediaFolder.internal(client, '/folder');
      final s1 = folder.propertiesChanged.listen((_) {});
      final s2 = folder.propertiesChanged.listen((_) {});
      expect(s2, isNotNull);
      s1.cancel();
      s2.cancel();
      folder.dispose();
    });

    test('BluezMediaItem stream allows multiple listeners', () {
      final item = BluezMediaItem.internal(client, '/item');
      final s1 = item.propertiesChanged.listen((_) {});
      final s2 = item.propertiesChanged.listen((_) {});
      expect(s2, isNotNull);
      s1.cancel();
      s2.cancel();
      item.dispose();
    });

    test('BluezMediaTransport stream allows multiple listeners', () {
      final transport = BluezMediaTransport.internal(client, '/transport');
      final s1 = transport.propertiesChanged.listen((_) {});
      final s2 = transport.propertiesChanged.listen((_) {});
      expect(s2, isNotNull);
      s1.cancel();
      s2.cancel();
      transport.dispose();
    });
  });

  // ── BluezMediaClient input validation (pure-Dart path) ───────────────────

  group('BluezMediaClient argument validation', () {
    // These validators run synchronously in Dart before any FFI call so they
    // are exercisable without a real native handle.

    // Note: _repeatModes and _shuffleModes are package-private constants.
    // The public API exposes the validation through the method, which throws
    // ArgumentError before reaching FFI. We exercise this via a fake client
    // that wraps the real static helpers — which is not possible without a
    // real client handle. Instead we document the expected surface:
    //
    //   client.setRepeat('/path', 'invalid') → ArgumentError
    //   client.setShuffle('/path', 'invalid') → ArgumentError
    //   client.getPlayerCoverArt('/path', 'relative/path') → ArgumentError
    //   client.getPlayerCoverArt('/path', '/abs', timeout: Duration.zero) → ArgumentError
    //   client.transportSetVolume('/path', -1) → RangeError
    //   client.transportSetVolume('/path', 128) → RangeError
    //
    // These are already exercised through the public API surface but can only
    // run with a real open client (handle != nullptr). They are captured here
    // as specification tests for when integration fixtures are added.
  });
}
