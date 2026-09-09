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

When the current `MediaPlayer1.Track` includes an experimental `ImgHandle`, the
image button downloads and displays cover art through BlueZ OBEX BIP. This
requires a running `obexd` with its experimental Image API enabled.
The native client maintains a matching BIP session while it is active, without
requiring `mpris-proxy`, and recreates it if `obexd` restarts. Its temporary
image directory is the example's cache and is removed when replaced or when the
app closes.

The UI follows ObjectManager property signals and provides an explicit refresh
action for properties that BlueZ does not signal.

The media items panel lists `MediaFolder1` folders and all `MediaItem1` objects
reported by BlueZ for the selected player, including each item's metadata and
trees such as
`NowPlaying/item1`, `NowPlaying/item2`, and `NowPlaying/item3`. Use the list
button on a folder to call `ListItems()` and merge any returned children into
the visible item tree.
