# bluez_media_native examples

These examples mirror the `bluez_native` CLI style. Run them from the package
root after BlueZ is running and the native library is available.

For CLI runs, the package resolves the native library in this order:

1. `BLUEZ_MEDIA_LIB=/absolute/path/to/libbluez_media_native.so`
2. executable-adjacent library paths
3. local build outputs such as `build/native/libbluez_media_native.so`
4. the system loader path, `libbluez_media_native.so`

## Local Player Registration

Register this process as a local media player object:

```sh
dart run example/register_player.dart list

dart run example/register_player.dart \
  --adapter /org/bluez/hci0 \
  --player /bluez_media/player0 \
  --name bluez_media_native \
  --hold 30
```

## MediaPlayer1 Control

Send standard player commands to a remote `org.bluez.MediaPlayer1` object:

```dart
final client = BluezMediaClient.create();
final player = client.player('/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0');

player.play();
player.pause();
player.next();

player.refresh();
print(player.status);
print(player.track);
```

```sh
dart run example/player_control.dart list
dart run example/player_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0 play
dart run example/player_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0 pause
dart run example/player_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0 stop
dart run example/player_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0 next
dart run example/player_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0 previous
dart run example/player_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0 props
```

## MediaControl1 Volume And Connectivity

Use the deprecated but still available `org.bluez.MediaControl1` controller
surface for volume adjustment and connected/player state snapshots:

```dart
final control = client.control('/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF');

control.volumeUp();
control.volumeDown();

control.refresh();
print(control.connected);
print(control.playerPath);
```

```sh
dart run example/media_control.dart list
dart run example/media_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF volume-up
dart run example/media_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF volume-down
dart run example/media_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF props
dart run example/media_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF watch --seconds 30
```

## AVRCP Media Browsing

Browse remote folders and playlist/media item trees via `MediaFolder1` and
`MediaItem1`:

```dart
final folder = client.folder('/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0');
final items = folder.listItems();

for (final item in items) {
  print('${item.name}: ${item.objectPath}');
  if (item.playable) {
    item.play();
  }
}
```

```sh
dart run example/media_browsing.dart list
dart run example/media_browsing.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0 pause
dart run example/media_browsing.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0 player-props
dart run example/media_browsing.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0 folder-props
dart run example/media_browsing.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0 list
dart run example/media_browsing.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0 search Coltrane
dart run example/media_browsing.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0 cd /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0/folder0
dart run example/media_browsing.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0/item0 item-props
dart run example/media_browsing.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0/item0 play-item
dart run example/media_browsing.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0/item0 add-now-playing
```

## MediaTransport1 Control

Acquire an active audio stream transport or set the absolute stream volume:

```dart
final transport = client.transport('/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/fd0');

transport.volume = 64; // Set volume directly

final result = transport.acquire();
try {
  print(result.fd);
} finally {
  result.close(); // Close the duplicated file descriptor.
  transport.release();
}
```

```sh
dart run example/transport_control.dart list
dart run example/transport_control.dart props /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/sep4/fd0
dart run example/transport_control.dart set_volume /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/sep4/fd0 64
dart run example/transport_control.dart acquire /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/sep4/fd0
dart run example/transport_control.dart try_acquire /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/sep4/fd0
dart run example/transport_control.dart release /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/sep4/fd0
```
