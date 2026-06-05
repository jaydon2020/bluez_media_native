# bluez_media_native

Native Dart and Flutter bindings for BlueZ media APIs on Linux, backed by
sdbus-c++.

## Supported Interfaces

- `org.bluez.Media1` player registration
- `org.bluez.MediaPlayer1` playback control and properties
- `org.bluez.MediaControl1` controller commands and connectivity
- `org.bluez.MediaFolder1` and `org.bluez.MediaItem1` media browsing
- `org.bluez.MediaTransport1` transport properties, volume, and acquisition

`org.bluez.MediaEndpoint1` XML and generated proxy definitions are included for
code generation, but endpoint registration is not currently exposed by the
public Dart API.

## Build

```sh
cmake -S native -B build/native -GNinja -DBUILD_TESTING=ON
cmake --build build/native
ctest --test-dir build/native --output-on-failure
```

For command-line Dart examples, set `BLUEZ_MEDIA_LIB` when the library is not
available through the system loader:

```sh
export BLUEZ_MEDIA_LIB="$PWD/build/native/libbluez_media_native.so"
dart run example/player_control.dart list
```

See [example/README.md](example/README.md) for registration, control, browsing,
and transport examples.

## Generate Bindings

```sh
dart run ffigen --config ffigen.yaml
./scripts/generate_proxies.sh
```

The checked-in files under `native/generated/` are generated sdbus-c++ protocol
artifacts. Runtime bridge classes currently use the hand-written wrappers under
`native/include/` and `native/src/`.
