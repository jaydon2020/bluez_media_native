# bluez_media_native

Native Dart and Flutter bindings for BlueZ media APIs on Linux, backed by
`dart:ffi` and `sdbus-c++`.

This package currently focuses on the tested BlueZ media receiver/control
surface: remote media player control, controller commands, and audio transport
inspection/acquisition.

## Features

- Control remote `org.bluez.MediaPlayer1` players: play, pause, stop, next,
  previous, repeat, shuffle, and property snapshots
- Use `org.bluez.MediaControl1` controller commands and connectivity snapshots
- Inspect and control `org.bluez.MediaTransport1` properties, volume, and file
  descriptor acquisition
- Receive native ObjectManager updates through a Dart `ReceivePort`
- Bundle the native library in Flutter Linux apps through the FFI plugin build

Planned for a future release:

- Register a local `org.bluez.MediaPlayer1` object through `org.bluez.Media1`
- Browse `org.bluez.MediaFolder1` and `org.bluez.MediaItem1` trees

`org.bluez.MediaEndpoint1` XML and generated proxy definitions are included for
code generation, but endpoint registration is not currently exposed by the
public Dart API.

## Platform Support

| Platform | MediaPlayer1 | MediaControl1 | MediaTransport1 | Media1 / Browsing |
|----------|--------------|---------------|-----------------|-------------------|
| Linux with BlueZ | Tested | Tested | Tested | Planned |
| macOS | No | No | No | No |
| Windows | No | No | No | No |

## Getting Started

### 1. Install system dependencies

Ubuntu/Debian:

```bash
sudo apt-get install cmake ninja-build clang libsystemd-dev pkg-config
```

Fedora:

```bash
sudo dnf install cmake ninja-build clang systemd-devel pkgconf-pkg-config
```

### 2. Add the package

```yaml
dependencies:
  bluez_media_native: ^0.0.1
```

### 3. Build the native library for CLI examples

Flutter Linux apps build and bundle the native library automatically through the
FFI plugin configuration. For plain Dart examples, build it manually:

```bash
cmake -S native -B build/native -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --parallel
```

If the library is not available through the system loader, point the Dart code
at the built shared object:

```bash
export BLUEZ_MEDIA_LIB="$PWD/build/native/libbluez_media_native.so"
```

### 4. Run an example

```bash
dart run example/player_control.dart list
dart run example/media_control.dart list
dart run example/transport_control.dart list
```

### 5. Run the Flutter example

```bash
cd example/flutter_ble_audio
flutter run -d linux
```

The Flutter example discovers BlueZ media objects and groups them by Bluetooth
device. It can refresh snapshots, send playback commands, adjust volume, set
repeat/shuffle modes, and inspect transport metadata.

## Quick Start

```dart
import 'package:bluez_media_native/bluez_media_native.dart';

Future<void> main() async {
  final client = BluezMediaClient.create();
  try {
    await client.ready;

    for (final player in client.players) {
      print('${player.name}: ${player.status}');
      print(player.track.map((p) => '${p.key}=${p.value}').join(', '));
    }

    if (client.players.isNotEmpty) {
      final player = client.players.first;
      player.play();
      player.refresh();
    }
  } finally {
    client.close();
  }
}
```

## API Surface

### BluezMediaClient

Top-level entry point. `BluezMediaClient.create()` initializes the Dart Native
DL API, opens a BlueZ system bus connection, starts the native event loop, and
caches media objects from BlueZ ObjectManager updates.

| Property / Method | Description |
|---|---|
| `ready` | Completes after the initial ObjectManager snapshot is cached |
| `players` | Cached `MediaPlayer1` proxies |
| `controls` | Cached `MediaControl1` proxies |
| `folders` | Cached `MediaFolder1` proxies, planned for future tested support |
| `items` | Cached `MediaItem1` proxies, planned for future tested support |
| `transports` | Cached `MediaTransport1` proxies |
| `transportAdded` / `transportRemoved` | Streams for transport lifecycle events |
| `registerPlayer()` / `unregisterPlayer()` | Register or remove a local media player, planned for future tested support |
| `getManagedObjects()` | Return a snapshot of known BlueZ media objects |
| `close()` | Stop native event processing and release cached proxies |

### Future: Media1 Local Player Registration

`org.bluez.Media1` is exposed on adapter paths such as `/org/bluez/hci0`.
Use it only when this Linux process wants to publish a local media player to
BlueZ. You do not need this to control a phone's remote
`/org/bluez/hci0/dev_.../avrcp/player0` object.

This API is planned for a future tested release.

```dart
import 'package:bluez_media_native/bluez_media_native.dart';

Future<void> main() async {
  final client = BluezMediaClient.create();
  try {
    client.registerPlayer(
      const BluezMediaPlayerRegistrationConfig(
        adapterPath: '/org/bluez/hci0',
        playerPath: '/bluez_media/player0',
        name: 'bluez_media_native',
        type: 'audio',
        browsable: true,
        searchable: true,
      ),
    );

    // Keep the process alive while the player is registered.
    await Future<void>.delayed(const Duration(seconds: 30));

    client.unregisterPlayer(
      adapterPath: '/org/bluez/hci0',
      playerPath: '/bluez_media/player0',
    );
  } finally {
    client.close();
  }
}
```

CLI equivalent:

```bash
dart run example/register_player.dart list
dart run example/register_player.dart \
  --adapter /org/bluez/hci0 \
  --player /bluez_media/player0 \
  --name bluez_media_native \
  --hold 30
```

### BluezMediaPlayer

Proxy for a remote `org.bluez.MediaPlayer1` object.

| Method / Property | Description |
|---|---|
| `play()`, `pause()`, `stop()` | Playback control |
| `next()`, `previous()` | Track navigation |
| `setRepeat()`, `setShuffle()` | Set BlueZ player modes when supported |
| `refresh()` | Fetch the latest property snapshot |
| `status`, `position`, `track`, `name` | Current player metadata |
| `propertiesChanged` | Stream of changed property names after updates |

### BluezMediaControl

Proxy for `org.bluez.MediaControl1`.

| Method / Property | Description |
|---|---|
| `play()`, `pause()`, `stop()` | Controller playback commands |
| `next()`, `previous()` | Controller track navigation |
| `volumeUp()`, `volumeDown()` | Controller volume commands |
| `fastForward()`, `rewind()` | Controller seek commands |
| `connected`, `playerPath` | Current control snapshot |

### Future: BluezMediaFolder and BluezMediaItem

AVRCP browsing helpers for `org.bluez.MediaFolder1` and
`org.bluez.MediaItem1`. These APIs are planned for a future tested release.

| Method / Property | Description |
|---|---|
| `folder.listItems()` | List child folders/items |
| `folder.search(value)` | Search under a folder |
| `folder.changeFolderPath(path)` | Change the current browsing folder |
| `item.play()` | Play a media item |
| `item.addToNowPlaying()` | Add an item to the now-playing list |
| `item.metadata`, `item.playable` | Item metadata snapshot |

### BluezMediaTransport

Proxy for `org.bluez.MediaTransport1`.

| Method / Property | Description |
|---|---|
| `refresh()` | Fetch the latest transport properties |
| `volume` | Get or set absolute transport volume |
| `acquire()` / `tryAcquire()` | Acquire a duplicated transport file descriptor |
| `release()` | Release the BlueZ transport |
| `uuid`, `codec`, `state`, `configuration` | Transport metadata |
| `BluezMediaAcquiredTransport.close()` | Close the duplicated file descriptor |

## Native Status Codes

The C ABI uses package-owned status codes, not D-Bus numeric error codes:

| Code | Name | Meaning |
|---|---|---|
| `0` | `BLUEZ_MEDIA_SUCCESS` | Operation succeeded |
| `-1` | `BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT` | Null handle/path, invalid capacity, or invalid input |
| `-2` | `BLUEZ_MEDIA_ERROR_BUFFER_TOO_SMALL` | Caller-provided output buffer is too small |
| `-3` | `BLUEZ_MEDIA_ERROR_OPERATION_FAILED` | D-Bus or native operation failed |
| `-4` | `BLUEZ_MEDIA_ERROR_UNSUPPORTED_SETTING` | BlueZ rejected repeat/shuffle setting |
| `-5` | `BLUEZ_MEDIA_ERROR_ALREADY_EXISTS` | Local player path is already registered |
| `-6` | `BLUEZ_MEDIA_ERROR_NOT_FOUND` | Local player path is not registered |

## Examples

- [`example/player_control.dart`](example/player_control.dart) - list and
  control remote `MediaPlayer1` objects
- [`example/media_control.dart`](example/media_control.dart) - use
  `MediaControl1` commands and snapshots
- [`example/transport_control.dart`](example/transport_control.dart) - inspect,
  acquire, release, and set volume on media transports
- [`example/flutter_ble_audio/`](example/flutter_ble_audio/) - Flutter Linux
  media-control UI

Future support examples:

- [`example/register_player.dart`](example/register_player.dart) - register this
  process as a local BlueZ media player
- [`example/media_browsing.dart`](example/media_browsing.dart) - browse folders,
  search, inspect items, and play media items

More command examples are available in [example/README.md](example/README.md).

## Project Structure

- `native/` - C/C++ source, CMake configuration, generated sdbus-c++ proxies,
  and BlueZ D-Bus XML interfaces
- `lib/` - Dart API and FFI codec/bindings
- `linux/` - Flutter Linux plugin build integration
- `example/` - CLI examples and the Flutter Linux demo app
- `test/` and `native/test/` - Dart codec tests and native wire-type tests

## Building And Testing

```bash
cmake -S native -B build/native -GNinja -DBUILD_TESTING=ON
cmake --build build/native --parallel
ctest --test-dir build/native --output-on-failure
```

Run Dart tests:

```bash
flutter test
```

## Generate Bindings And Proxies

Regenerate Dart FFI bindings from `native/include/bluez_media_native.h`:

```bash
dart run ffigen --config ffigen.yaml
```

Regenerate sdbus-c++ proxy headers from the BlueZ XML files:

```bash
./scripts/generate_proxies.sh
```

The checked-in files under `native/generated/` are generated protocol artifacts.
Runtime bridge classes currently use the hand-written wrappers under
`native/include/` and `native/src/`.

## Troubleshooting

### BlueZ is not running

```bash
systemctl status bluetooth
sudo systemctl start bluetooth
```

### No media objects are listed

Media objects appear only when BlueZ exposes them for connected devices. Pair and
connect an audio-capable device, then check:

```bash
bluetoothctl show
bluetoothctl devices Connected
busctl tree org.bluez
```

### `org.bluez.Error.NotReady`

The adapter may be powered off or blocked:

```bash
bluetoothctl power on
rfkill list bluetooth
sudo rfkill unblock bluetooth
```

### Permission errors

Some BlueZ operations require local system bus permissions. During development,
try running the CLI example with elevated privileges while preserving the native
library path:

```bash
sudo BLUEZ_MEDIA_LIB="$PWD/build/native/libbluez_media_native.so" \
  dart run example/player_control.dart list
```

## Flutter Help

For general Flutter plugin and Linux desktop help, see the
[Flutter documentation](https://docs.flutter.dev).
