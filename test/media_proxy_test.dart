// SPDX-License-Identifier: Apache-2.0

import 'package:bluez_media_native/bluez_media_native.dart';
import 'package:test/test.dart';

void main() {
  final client = _FakeClient();

  test('player exposes metadata and reports changed properties', () async {
    final player = BluezMediaPlayer.internal(client, '/player');
    final changed = expectLater(
      player.propertiesChanged,
      emits([
        'Equalizer',
        'Status',
        'Position',
        'Track',
        'Scan',
        'Name',
        'Type',
        'Subtype',
        'Device',
        'Browsable',
        'Searchable',
        'Playlist',
        'ObexPort',
      ]),
    );

    player.updateProps(
      const BlueZMediaPlayerProps(
        objectPath: '/player',
        equalizer: 'on',
        status: 'playing',
        position: 42,
        track: [BlueZMediaProperty(key: 'ImgHandle', value: 'image-1')],
        scan: 'alltracks',
        name: 'Phone',
        type: 'audio',
        subtype: 'player',
        device: '/device',
        browsable: true,
        searchable: true,
        playlist: '/playlist',
        obexPort: 7,
      ),
    );

    await changed;
    expect(player.imageHandle, 'image-1');
    expect(player.equalizer, 'on');
    expect(player.scan, 'alltracks');
    expect(
      () => player.track.add(const BlueZMediaProperty(key: 'x', value: 'y')),
      throwsUnsupportedError,
    );
    player.dispose();
  });

  test('item compares metadata by value', () async {
    final item = BluezMediaItem.internal(client, '/item');
    item.updateProps(
      const BlueZMediaItemProps(
        objectPath: '/item',
        metadata: [BlueZMediaProperty(key: 'Title', value: 'First')],
      ),
    );
    final changed = expectLater(item.propertiesChanged, emits(['Metadata']));

    item.updateProps(
      const BlueZMediaItemProps(
        objectPath: '/item',
        metadata: [BlueZMediaProperty(key: 'Title', value: 'Second')],
      ),
    );

    await changed;
    expect(item.metadata.single.value, 'Second');
    item.dispose();
  });

  test('transport reports configuration changes by value', () async {
    final transport = BluezMediaTransport.internal(client, '/transport');
    transport.updateProps(
      const BlueZMediaTransportProps(
        objectPath: '/transport',
        configuration: [1, 2],
      ),
    );
    final changed = expectLater(
      transport.propertiesChanged,
      emits(['State', 'Volume', 'Configuration']),
    );

    transport.updateProps(
      const BlueZMediaTransportProps(
        objectPath: '/transport',
        state: 'active',
        volume: 90,
        configuration: [1, 3],
      ),
    );

    await changed;
    expect(transport.configuration, [1, 3]);
    transport.dispose();
  });

  test('transport refresh reports changed properties', () async {
    final transport = BluezMediaTransport.internal(client, '/transport');
    final changed = expectLater(
      transport.propertiesChanged,
      emits(['State', 'Volume']),
    );

    await transport.refresh();

    await changed;
    expect(transport.state, 'active');
    expect(transport.volume, 64);
    transport.dispose();
  });
}

class _FakeClient implements BluezMediaClient {
  @override
  Future<BlueZMediaTransportProps> getMediaTransportProperties(
    String transportPath,
  ) async {
    return BlueZMediaTransportProps(
      objectPath: transportPath,
      state: 'active',
      volume: 64,
    );
  }

  @override
  dynamic noSuchMethod(Invocation invocation) => super.noSuchMethod(invocation);
}
