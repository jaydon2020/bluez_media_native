# bluez_media_native

Native Dart and Flutter bindings for BlueZ media APIs on Linux, backed by `dart:ffi` and `sdbus-c++`.

This package provides a robust, asynchronous interface to the BlueZ media receiver and control surfaces, including remote media player control, controller commands, and audio transport inspection/acquisition.

## Table of Contents
- [Features](#features)
- [Platform Support](#platform-support)
- [Installation](#installation)
- [Dynamic Linking](#dynamic-linking)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
- [Troubleshooting](#troubleshooting)
- [Building from Source](#building-from-source)

## Features

- **Media Player Control**: Control remote `org.bluez.MediaPlayer1` players (play, pause, stop, next, previous, repeat, shuffle) and retrieve property snapshots.
- **Media Controller Commands**: Utilize `org.bluez.MediaControl1` commands and monitor connectivity.
- **Audio Transport Inspection**: Inspect `org.bluez.MediaTransport1` properties, adjust volume, and acquire file descriptors.
- **Native Event Integration**: Receive asynchronous ObjectManager updates safely through a Dart `ReceivePort`.
- **Seamless Bundling**: Automatically build and bundle the native C++ library in Dart and Flutter Linux apps using Dart Native Assets (`hook/build.dart`).
- **Media Browsing**: Browse and search `org.bluez.MediaFolder1` and `org.bluez.MediaItem1` hierarchies.
- **Cover Art Retrieval**: Download AVRCP cover art through the experimental BlueZ OBEX BIP Image API.
- **Local Player Registration**: Register a local MPRIS player object through `org.bluez.Media1`.

## Platform Support

| Platform | MediaPlayer1 | MediaControl1 | MediaTransport1 | Media1 / Browsing |
| :--- | :---: | :---: | :---: | :---: |
| **Linux (BlueZ)** | ✅ Tested | ✅ Tested | ✅ Tested | ✅ Supported |
| **macOS** | ❌ | ❌ | ❌ | ❌ |
| **Windows** | ❌ | ❌ | ❌ | ❌ |

## Installation

### 1. System Dependencies

Before adding the package, ensure your Linux environment has the required build tools and D-Bus headers installed.

**Ubuntu / Debian:**
```bash
sudo apt-get update
sudo apt-get install cmake ninja-build clang libsystemd-dev pkg-config
```

**Fedora:**
```bash
sudo dnf install cmake ninja-build clang systemd-devel pkgconf-pkg-config
```

### 2. Add Package

Add the package to your `pubspec.yaml`:
```yaml
dependencies:
  bluez_media_native: ^0.3.1
```

## Dynamic Linking

The shared library is named `libbluez_media_native.so`. Dart Native Assets
builds and bundles it automatically when this package is used normally. To use
a manually built library instead, build it and set `BLUEZ_MEDIA_LIB` to its
absolute path:

```bash
cmake -S native -B build/native -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --parallel

BLUEZ_MEDIA_LIB="$PWD/build/native/libbluez_media_native.so" \
  dart run example/player_control.dart
```

To set it for the current shell and every command started from that shell:

```bash
export BLUEZ_MEDIA_LIB="$PWD/build/native/libbluez_media_native.so"
dart run example/player_control.dart
```

Plain `echo BLUEZ_MEDIA_LIB=...` only prints the value. To keep the variable
across logins, write the `export` command to your shell profile and source it:

```bash
echo 'export BLUEZ_MEDIA_LIB=/absolute/path/libbluez_media_native.so' >> ~/.profile
source ~/.profile
```

For a Flutter or AGL process, provide the same variable in its launch
environment:

```ini
[Service]
Environment=BLUEZ_MEDIA_LIB=/usr/lib/libbluez_media_native.so
```

Alternatively, place the library beside the application's `libapp.so`, add its
directory to `LD_LIBRARY_PATH`, or install it in the system loader path:

```bash
sudo install -Dm755 build/native/libbluez_media_native.so \
  /usr/local/lib/libbluez_media_native.so
sudo ldconfig
```

The Dart loader searches `BLUEZ_MEDIA_LIB`, the system loader path, directories
containing already loaded libraries, Native Assets, and local `build` output
directories, in that order. Use `ldd` to verify the library's runtime
dependencies:

```bash
ldd build/native/libbluez_media_native.so
```

Native C or C++ consumers can link against the exported C ABI directly:

```bash
c++ app.cpp -Inative/include -Lbuild/native \
  -Wl,-rpath,"$PWD/build/native" -lbluez_media_native -o app
```

## Quick Start

```dart
import 'package:bluez_media_native/bluez_media_native.dart';

Future<void> main() async {
  // Initializes the native D-Bus connection and caches the initial ObjectManager snapshot.
  final client = await BluezMediaClient.create();
  
  try {
    // Iterate through connected media players
    for (final player in client.players) {
      print('Player: ${player.name} | Status: ${player.status}');
      print(player.track.map((p) => '${p.key}=${p.value}').join(', '));
    }

    // Play and fetch cover art from the first available player
    if (client.players.isNotEmpty) {
      final player = client.players.first;
      await player.play();
      await player.refresh();
      
      if (player.imageHandle.isNotEmpty) {
        // Will download to the specified absolute path
        await player.getCoverArt('/tmp/bluez-cover-art.jpg');
      }
    }
  } finally {
    // Safely shutdown native event processing
    await client.close();
  }
}
```

## API Reference

### `BluezMediaClient`
The top-level entry point. `await BluezMediaClient.create()` initializes the native D-Bus integration, starts the native event loop, and returns after caching the initial ObjectManager snapshot.

| API | Description |
| :--- | :--- |
| `players`, `controls` | Cached `MediaPlayer1` and `MediaControl1` proxies |
| `folders`, `items` | Cached `MediaFolder1` and `MediaItem1` proxies |
| `transports` | Cached `MediaTransport1` proxies |
| `playerAdded`, `playerRemoved`, etc. | Dart Streams for lifecycle events across all media interfaces |
| `getManagedObjects()` | Returns a snapshot of known BlueZ media objects |
| `registerPlayer()`, `unregisterPlayer()`| Register/remove a local MPRIS player |
| `close()` | Stop native event processing and release cached proxies cleanly |

### Remote Media Proxies
- **`BluezMediaPlayer`**: Proxy for `MediaPlayer1`. Provides `play()`, `pause()`, `next()`, `setRepeat()`, `setShuffle()`, `refresh()`, and exposes a `propertiesChanged` stream.
- **`BluezMediaControl`**: Proxy for `MediaControl1`. Provides volume and playback control commands.
- **`BluezMediaFolder` / `BluezMediaItem`**: AVRCP browsing helpers. Enables folder navigation (`listItems()`, `changeFolderPath()`) and item interaction (`play()`, `addToNowPlaying()`).
- **`BluezMediaTransport`**: Proxy for `MediaTransport1`. Enables inspecting transport state, setting volume, and acquiring duplicated file descriptors (`acquire()`).

### Local Player Registration (`org.bluez.Media1`)

> **Note on Registration:** `registerPlayer` currently exports an **experimental, inert MPRIS object** for registration testing. It does not connect to a Dart/Flutter audio player or expose an application command stream. Remote BlueZ player control and browsing are completely unaffected by this limitation.

Use this only when your Linux process wants to publish a local media player to BlueZ (e.g. on `/org/bluez/hci0`).

```dart
final client = await BluezMediaClient.create();
await client.registerPlayer(
  const BluezMediaPlayerRegistrationConfig(
    adapterPath: '/org/bluez/hci0',
    playerPath: '/bluez_media/player0',
    name: 'bluez_media_native',
    type: 'Audio',
  ),
);
```

## Troubleshooting

- **Cover art is unavailable**: Cover art requires a running BlueZ `obexd` with the experimental Image API enabled. While the client is active, it maintains a matching BIP session for each player so BlueZ can include `ImgHandle` in subsequent `MediaPlayer1.Track` updates; an already-playing track may not gain a handle until its metadata changes. `getCoverArt` requests the native image first and falls back to the thumbnail or a negotiated native description. The destination path must be absolute and not already exist.
- **No media objects listed**: Media objects only appear when BlueZ exposes them for connected devices. Ensure your device is paired, connected, and playing audio (`bluetoothctl devices Connected`).
- **`org.bluez.Error.NotReady`**: Your Bluetooth adapter may be powered off or blocked. Check with `bluetoothctl power on` or `sudo rfkill unblock bluetooth`.
- **Permission errors**: Some BlueZ operations require local system bus permissions. Ensure your user has the correct D-Bus permissions or try running with elevated privileges during development.

## Building from Source

The package uses Dart Native Assets to compile automatically. However, for direct native development or testing:

**Build Native C++ Library:**
```bash
cmake -S native -B build/native -GNinja -DBUILD_TESTING=ON
cmake --build build/native --parallel
```

**Run Test Suites:**
```bash
# Run C++ unit tests
ctest --test-dir build/native --output-on-failure

# Run Dart unit tests
dart test
```

For advanced integration testing (D-Bus daemon resync, memory tracking, etc.), see the `/scripts` directory for specialized ASAN and isolated D-Bus execution scripts.
