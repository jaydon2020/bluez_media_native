# bluez_media_native examples

These examples mirror the `bluez_native` CLI style. Run them from the package
root after BlueZ is running and the native library is available.

## Local Player Registration

Register this process as a local media player object:

```sh
dart run example/register_player.dart \
  --adapter /org/bluez/hci0 \
  --player /bluez_media/player0 \
  --name bluez_media_native \
  --status stopped \
  --hold 30
```

## MediaPlayer1 Control

Send standard player commands to a remote `org.bluez.MediaPlayer1` object:

```sh
dart run example/player_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0 play
dart run example/player_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0 pause
dart run example/player_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0 stop
dart run example/player_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0 next
dart run example/player_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0 previous
dart run example/player_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0 props
```

## MediaControl1 Volume And Connectivity

Use the deprecated but still available `org.bluez.MediaControl1` controller
surface for volume adjustment and connected/player state snapshots:

```sh
dart run example/media_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF volume-up
dart run example/media_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF volume-down
dart run example/media_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF props
dart run example/media_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF watch --seconds 30
```

## AVRCP Media Browsing

Browse remote folders and playlist/media item trees via `MediaFolder1` and
`MediaItem1`:

```sh
dart run example/media_browsing.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0 folder-props
dart run example/media_browsing.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0 list
dart run example/media_browsing.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0 search Coltrane
dart run example/media_browsing.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0 cd /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0/folder0
dart run example/media_browsing.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0/item0 item-props
dart run example/media_browsing.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0/item0 play-item
dart run example/media_browsing.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0/item0 add-now-playing
```
