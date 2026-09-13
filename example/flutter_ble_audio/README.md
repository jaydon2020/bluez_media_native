# flutter_ble_audio

Flutter Linux example for controlling Bluetooth audio media through
`bluez_media_native`.

Run from this directory. Native OBEX is the default cover-art backend:

```sh
flutter run -d linux
```

Or use artwork already published by a running `mpris-proxy`:

```sh
flutter run -d linux --dart-define=BLUEZ_MEDIA_COVER_ART=mpris
```

The app discovers BlueZ media objects, groups them by Bluetooth device, and
lets you choose one device from the top selector. It shows which media
interfaces are available for that device, then exposes playback, repeat,
shuffle, controller volume, transport volume, and transport metadata controls
when BlueZ reports support for them.

When the current `MediaPlayer1.Track` includes an experimental `ImgHandle`, the
image button downloads and displays cover art through BlueZ OBEX BIP. This
requires a running `obexd` with its experimental Image API enabled.
Native mode reuses an existing matching BIP session or creates one as needed,
then requests a thumbnail directly. MPRIS mode only reads the file published by
`mpris-proxy`; it never falls back to OBEX or reads incomplete artwork during
refresh. The app does not detect or switch between modes. Flutter displays
animated GIF artwork without conversion. Its temporary image directory is
removed when replaced or when the app closes.

The UI follows ObjectManager property signals and provides an explicit refresh
action for properties that BlueZ does not signal.

The media items panel lists `MediaFolder1` folders and all `MediaItem1` objects
reported by BlueZ for the selected player, including each item's metadata and
trees such as
`NowPlaying/item1`, `NowPlaying/item2`, and `NowPlaying/item3`. Use the list
button on a folder to call `ListItems()` and merge any returned children into
the visible item tree.
