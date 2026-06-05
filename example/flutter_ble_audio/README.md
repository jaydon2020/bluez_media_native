# flutter_ble_audio

Flutter Linux example for controlling Bluetooth audio media through
`bluez_media_native`.

Run from this directory:

```sh
flutter run -d linux
```

The app discovers BlueZ media objects, groups them by Bluetooth device, and
lets you choose one device from the top selector. It shows which media
interfaces are available for that device, then exposes playback, repeat,
shuffle, controller volume, transport volume, and transport metadata controls
when BlueZ reports support for them.
