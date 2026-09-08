import 'dart:async';
import 'package:bluez_media_native/bluez_media_native.dart';
import 'package:test/test.dart';

class _Client implements BluezMediaClient {
  final reply = Completer<BlueZMediaPlayerProps>();
  @override
  Future<BlueZMediaPlayerProps> getPlayerProperties(String path) =>
      reply.future;
  @override
  dynamic noSuchMethod(Invocation invocation) => super.noSuchMethod(invocation);
}

void main() {
  test('refresh cannot overwrite a newer property event', () async {
    final client = _Client();
    final player = BluezMediaPlayer.internal(client, '/player');
    final refresh = player.refresh();
    player.updateProps(
      const BlueZMediaPlayerProps(objectPath: '/player', status: 'playing'),
    );
    client.reply.complete(
      const BlueZMediaPlayerProps(objectPath: '/player', status: 'paused'),
    );
    expect((await refresh).status, 'playing');
    expect(player.status, 'playing');
    player.dispose();
  });

  test('removal discards a refresh reply and rejects stale commands', () async {
    final client = _Client();
    final player = BluezMediaPlayer.internal(client, '/player');
    final refresh = player.refresh();
    player.dispose();
    client.reply.complete(
      const BlueZMediaPlayerProps(objectPath: '/player', status: 'playing'),
    );
    await refresh;
    expect(player.status, isEmpty);
    expect(player.isDisposed, isTrue);
    expect(player.play, throwsStateError);
    expect(player.refresh(), throwsStateError);
    player.dispose();
  });

  test('all disposed proxy types ignore late property events', () {
    final client = _Client();
    final control = BluezMediaControl.internal(client, '/control')..dispose();
    final folder = BluezMediaFolder.internal(client, '/folder')..dispose();
    final item = BluezMediaItem.internal(client, '/item')..dispose();
    final transport = BluezMediaTransport.internal(client, '/transport')
      ..dispose();
    control.updateProps(
      const BlueZMediaControlProps(objectPath: '/control', connected: true),
    );
    folder.updateProps(
      const BlueZMediaFolderProps(objectPath: '/folder', name: 'late'),
    );
    item.updateProps(
      const BlueZMediaItemProps(objectPath: '/item', name: 'late'),
    );
    transport.updateProps(
      const BlueZMediaTransportProps(objectPath: '/transport', volume: 50),
    );
    expect(control.connected, isFalse);
    expect(folder.name, isEmpty);
    expect(item.name, isEmpty);
    expect(transport.volume, 0);
    expect(control.play, throwsStateError);
    expect(() => folder.changeFolderPath('/new'), throwsStateError);
    expect(item.play, throwsStateError);
    expect(transport.release, throwsStateError);
  });
}
