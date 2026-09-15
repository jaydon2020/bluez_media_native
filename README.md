# bluez_media_native

`bluez_media_native` provides asynchronous Dart and Flutter bindings for the
BlueZ media APIs on Linux. It uses `dart:ffi` and `sdbus-c++` to expose remote
media control, AVRCP browsing, local player registration, audio transport
access, and Bluetooth cover-art retrieval through a single typed API.

The package tracks BlueZ objects through `org.freedesktop.DBus.ObjectManager`,
caches their current properties, and publishes object lifecycle and property
changes as Dart streams. The native library is built and bundled automatically
through Dart Native Assets.

## Table of Contents

- [Features](#features)
- [Platform Support](#platform-support)
- [Requirements](#requirements)
- [Installation](#installation)
- [Dynamic Linking](#dynamic-linking)
- [Quick Start](#quick-start)
- [Media Browsing](#media-browsing)
- [API Reference](#api-reference)
- [Cover Art](#cover-art)
- [Building from Source](#building-from-source)
- [Acknowledgements](#acknowledgements)

## Features

| BlueZ interface | Supported methods and properties |
| :--- | :--- |
| `org.bluez.MediaPlayer1` | `Play`, `Pause`, `Stop`, `Next`, `Previous`, `FastForward`, `Rewind`, `Equalizer`, `Repeat`, `Shuffle`, `Scan`, `Status`, `Position`, `Track`, `Device`, `Name`, `Type`, `Subtype`, `Browsable`, `Searchable`, `Playlist`, and `ObexPort` |
| `org.bluez.MediaControl1` | `Play`, `Pause`, `Stop`, `Next`, `Previous`, `VolumeUp`, `VolumeDown`, `FastForward`, `Rewind`, `Connected`, and `Player` |
| `org.bluez.MediaTransport1` | `Acquire`, `TryAcquire`, `Release`, `Device`, `UUID`, `Codec`, `Configuration`, `State`, `Delay`, `Volume`, and `Endpoint` |
| `org.bluez.MediaFolder1` | `Search`, `ListItems`, `ChangeFolder`, `NumberOfItems`, and `Name` |
| `org.bluez.MediaItem1` | `Play`, `AddtoNowPlaying`, `Player`, `Name`, `Type`, `FolderType`, `Playable`, and `Metadata` |
| `org.bluez.Media1` | `RegisterPlayer` and `UnregisterPlayer` |
| `org.bluez.obex.Client1` | `CreateSession` and `RemoveSession` for native cover art |
| `org.bluez.obex.Image1` | `GetThumbnail`, `Properties`, and `Get` |
| `org.bluez.obex.Transfer1` | `Status` and `Cancel` |

## Platform Support

| Platform | MediaPlayer1 | MediaControl1 | MediaTransport1 | Media1 registration | Browsing | Cover art |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| Linux with BlueZ | Tested | Tested | Tested | Tested | Tested | Tested |
| macOS | Not supported | Not supported | Not supported | Not supported | Not supported | Not supported |
| Windows | Not supported | Not supported | Not supported | Not supported | Not supported | Not supported |

"Tested" refers to the package's native, Dart, and isolated D-Bus test suites.
Hardware behavior still depends on the BlueZ build, Bluetooth adapter, remote
device, and the profiles implemented by that device.

## Requirements

- Linux with BlueZ and a running system D-Bus.
- Dart SDK 3.10.1 or later.
- CMake 3.22 or later, Ninja, Clang, and systemd development headers when
  building the native library.
- A BlueZ build exposing the relevant media interfaces.
- For cover art in either mode, `bluetoothd` started with `--experimental`
  (short form `-E`), or `[General] Experimental = true` configured in
  `main.conf`. This enables the experimental `MediaPlayer1.ObexPort` and
  `ImgHandle` properties used by AVRCP cover art.
- For native cover art, a user-session `obexd` built with the experimental BIP
  Image client API and reachable through `DBUS_SESSION_BUS_ADDRESS`.
- For MPRIS cover art, a running BlueZ `mpris-proxy` and `obexd` on the same
  user-session bus.

BlueZ media browsing and cover art are remote-device capabilities. Their
interfaces or metadata may be absent even when normal A2DP playback works.

## Installation

### 1. Install system dependencies

Before adding the package, ensure your Linux environment has the required
build tools and D-Bus headers installed.

Ubuntu or Debian:

```bash
sudo apt-get update
sudo apt-get install cmake ninja-build clang libsystemd-dev pkg-config
```

Fedora:

```bash
sudo dnf install cmake ninja-build clang systemd-devel pkgconf-pkg-config
```

### 2. Add the package

Add the package to your `pubspec.yaml`:

```yaml
dependencies:
  bluez_media_native: ^0.3.0
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

## Quick Start

```dart
import 'package:bluez_media_native/bluez_media_native.dart';

Future<void> main() async {
  final client = await BluezMediaClient.create(
    coverArtMode: BluezMediaCoverArtMode.native,
  );

  try {
    for (final player in client.players) {
      print('Player: ${player.name} | Status: ${player.status}');
      print(player.track.map((p) => '${p.key}=${p.value}').join(', '));
    }

    if (client.players.isNotEmpty) {
      final player = client.players.first;
      await player.play();
      await player.refresh();

      if (player.obexPort != 0) {
        await player.getCoverArt('/tmp/bluez-cover-art.jpg');
      }
    }
  } finally {
    await client.close();
  }
}
```

Always close the client. Closing it stops native event processing, disposes
cached proxies, releases retained descriptors, and removes native-owned OBEX
sessions.

## Media Browsing

Browsing uses `org.bluez.MediaFolder1` and `org.bluez.MediaItem1`. Available
folders and items are cached on `BluezMediaClient`, and additions, removals,
and property updates are delivered through streams.

```dart
import 'package:bluez_media_native/bluez_media_native.dart';

final client = await BluezMediaClient.create(
  coverArtMode: BluezMediaCoverArtMode.disabled,
);

try {
  for (final folder in client.folders) {
    final items = await folder.listItems();
    for (final item in items) {
      print('${item.name}: ${item.objectPath}');
      if (item.playable) {
        await item.play();
      }
    }
  }
} finally {
  await client.close();
}
```

Supported browsing operations include:

- Listing the current folder with `listItems()`.
- Searching with `search()`.
- Navigating with `changeFolder()` or `changeFolderPath()`.
- Playing an item with `play()`.
- Adding an item with `addToNowPlaying()`.
- Refreshing folder and item property snapshots.
- Retrieving cover art associated with item metadata.

Run `dart run example/media_browsing.dart --help` for command-line examples.

## API Reference

### `BluezMediaClient`

The top-level entry point. `BluezMediaClient.create` requires an explicit
`BluezMediaCoverArtMode.disabled`, `.native`, or `.mpris` selection, starts the
native D-Bus integration, and returns after caching the initial ObjectManager
snapshot. It never probes for `mpris-proxy` or changes mode.

| API | Description |
| :--- | :--- |
| `players`, `controls` | Cached `MediaPlayer1` and `MediaControl1` proxies |
| `folders`, `items` | Cached `MediaFolder1` and `MediaItem1` proxies |
| `transports` | Cached `MediaTransport1` proxies |
| `playerAdded`, `playerRemoved`, etc. | Dart Streams for lifecycle events across all media interfaces |
| `getManagedObjects()` | Returns a snapshot of known BlueZ media objects |
| `registerPlayer()`, `unregisterPlayer()` | Register or remove a local MPRIS player through `Media1` |
| `close()` | Stop native event processing and release cached proxies cleanly |

### Remote media proxies

- `BluezMediaPlayer` provides `MediaPlayer1` playback commands, playback
  options, track metadata, cover art, refreshes, and property-change events.
- `BluezMediaControl` provides `MediaControl1` connection, volume, and
  playback commands.
- `BluezMediaFolder` and `BluezMediaItem` provide tested AVRCP browsing,
  searching, navigation, item playback, and item cover art.
- `BluezMediaTransport` provides `MediaTransport1` snapshots, volume control,
  and duplicated file descriptors through `acquire()` and `tryAcquire()`.
  Close each `BluezMediaAcquiredTransport` after use and call `release()` when
  the BlueZ transport itself should be released.

### Local player registration (`org.bluez.Media1`)

`registerPlayer` and `unregisterPlayer` are tested against the `Media1`
registration lifecycle. The currently exported local MPRIS object is
experimental and inert: it does not connect to a Dart or Flutter audio engine
and does not expose incoming player commands as a Dart stream. This limitation
does not affect remote BlueZ media control or browsing.

Use this only when the Linux process needs to publish a local media player to
BlueZ, for example on `/org/bluez/hci0`.

```dart
final client = await BluezMediaClient.create(
  coverArtMode: BluezMediaCoverArtMode.disabled,
);
await client.registerPlayer(
  const BluezMediaPlayerRegistrationConfig(
    adapterPath: '/org/bluez/hci0',
    playerPath: '/bluez_media/player0',
    name: 'bluez_media_native',
    type: 'Audio',
  ),
);

await client.unregisterPlayer(
  adapterPath: '/org/bluez/hci0',
  playerPath: '/bluez_media/player0',
);
```

## Cover Art

Cover art is supported for both `BluezMediaPlayer` and `BluezMediaItem`. The
backend is selected explicitly when the client is created:

| Mode | Behavior | External service |
| :--- | :--- | :--- |
| `disabled` | Disables cover-art session management and retrieval. | None |
| `native` | Reuses a matching OBEX BIP session or creates and owns one, requests `Image1.GetThumbnail` first, then falls back to the preferred native image variant. | `obexd` |
| `mpris` | Reads the completed local artwork file published in `mpris:artUrl` by BlueZ `mpris-proxy`. It does not perform direct OBEX requests. | `obexd` and `mpris-proxy` |

Modes are never detected automatically and never fall back to each other.
Choose one session manager for production use. Running independent clients
that concurrently create the same BIP session can expose session-lifecycle
bugs in some BlueZ versions.

### Native mode

```dart
import 'dart:io';

import 'package:bluez_media_native/bluez_media_native.dart';

final client = await BluezMediaClient.create(
  coverArtMode: BluezMediaCoverArtMode.native,
);

try {
  final player = client.players.first;
  final directory = await Directory.systemTemp.createTemp('bluez-art-');
  final path = await player.getCoverArt('${directory.path}/cover-art');
  final bytes = await File(path).readAsBytes();
  print('Received ${bytes.length} bytes from $path');
} finally {
  await client.close();
}
```

Native mode requires a nonzero `MediaPlayer1.ObexPort`. The corresponding
`ImgHandle` normally appears in player track or item metadata after the BIP
session becomes active. A track that was already playing may not receive a
handle until its metadata changes; changing tracks is a common way to trigger
that update.

`getCoverArt` accepts an absolute destination path that must not already exist.
It waits up to 15 seconds by default and returns the completed path. Override
the timeout when necessary:

```dart
final path = await player.getCoverArt(
  '/tmp/cover-art',
  timeout: const Duration(seconds: 30),
);
```

Use `getCoverArtFromExistingSession` when another process owns the BIP session
and this application must not create or remove it:

```dart
final path = await player.getCoverArtFromExistingSession('/tmp/cover-art');
```

The same methods are available on `BluezMediaItem`. The package preserves the
downloaded bytes without decoding or re-encoding them, so JPEG, PNG, and
animated GIF artwork remain intact. The caller owns the resulting file, its
decoding, and its cache lifetime.

### MPRIS mode

```dart
import 'dart:io';

import 'package:bluez_media_native/bluez_media_native.dart';

final client = await BluezMediaClient.create(
  coverArtMode: BluezMediaCoverArtMode.mpris,
);

try {
  final player = client.players.first;
  String? itemPath;
  for (final property in player.track) {
    if (property.key == 'Item') {
      itemPath = property.value;
      break;
    }
  }
  if (itemPath != null) {
    final bytes = await client.getMprisCoverArt(itemPath);
    await File('/tmp/cover-art').writeAsBytes(bytes, flush: true);
  }
} finally {
  await client.close();
}
```

MPRIS mode accepts only a local `file://` artwork URL published by the BlueZ
`mpris-proxy` process. Empty, incomplete, missing, non-regular, symbolic-link,
or oversized files are rejected. A request can temporarily fail while
`mpris-proxy` is still writing the file; retry after the matching metadata
update rather than switching backends automatically.

The command-line example exposes both explicit modes:

```bash
dart run example/media_cover_art.dart /tmp/cover-art.jpg --native
dart run example/media_cover_art.dart /tmp/cover-art.jpg --mpris
```

## Building from Source

The package uses Dart Native Assets for normal consumer builds. For direct
native development or testing:

Build the native C++ library:

```bash
cmake -S native -B build/native -GNinja -DBUILD_TESTING=ON
cmake --build build/native --parallel
```

Run the native and Dart test suites:

```bash
ctest --test-dir build/native --output-on-failure
dart test
```

Run the isolated private-D-Bus integration suite, Native Assets consumer test,
sanitizers, coverage, and static analysis as needed:

```bash
./scripts/test_dbus.sh
./scripts/test_asset.sh
./scripts/asan.sh
./scripts/coverage.sh
./scripts/clang_format.sh
./scripts/clang_tidy.sh
```

The D-Bus integration suite covers initial object discovery, property changes,
daemon disappearance and replacement, media browsing, local `Media1`
registration, cover-art modes, and client lifecycle behavior without depending
on the host's live BlueZ service.

## Acknowledgements

- [bluez_native](https://github.com/jwinarske/bluez_native) (Apache-2.0): the
  Native Assets build hook, library loader, glaze binary codec, FFI handle
  lifetime design, and development scripts are derived from this project.
- [appstream_dart](https://github.com/meta-flutter/appstream_dart): the
  original pattern for the build hook and library loader.
- [sdbus-c++](https://github.com/Kistler-Group/sdbus-cpp) (LGPL-2.1): the D-Bus
  C++ binding used by the native library.
