# flutter_ble_audio

Flutter Linux example for controlling Bluetooth audio media through
`bluez_media_native`.

Run from this directory:

```sh
flutter run -d linux
```

The app discovers BlueZ `MediaPlayer1` and `MediaControl1` objects, lets you
select the active object paths, shows playback and track metadata, and exposes
Play/Pause/Stop/Next/Previous plus volume controls.
