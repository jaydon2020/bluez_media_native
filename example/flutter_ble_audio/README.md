# flutter_ble_audio

Flutter Linux example for controlling Bluetooth audio media through
`bluez_media_native`.

Run from this directory:

```sh
flutter run -d linux
```

The app discovers BlueZ `MediaPlayer1` and `MediaControl1` objects, lets you
select the active object paths, shows playback and track metadata, and exposes
Play/Pause/Stop/Next/Previous plus volume controls. Use the swap button to move
to the next discovered player/controller pair, and use Repeat to set
`MediaPlayer1.Repeat` to `off` or `singletrack`. Use Shuffle to set
`MediaPlayer1.Shuffle` to `off` or `alltracks`.
