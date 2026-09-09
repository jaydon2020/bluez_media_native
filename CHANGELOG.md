# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
- Kept one OBEX BIP cover-art session per device while media clients are active,
  recreated sessions after `obexd` restarts, and requested the native image
  before optional thumbnail or negotiated-format fallbacks.
## [0.3.0] - 2026-09-08

### Added
- Session reuse cover art APIs (`getCoverArtFromExistingSession`) to reuse existing OBEX sessions (e.g. `mpris-proxy`) without managing session lifetimes.
- MediaItem cover art acquisition across native C ABI and Dart APIs (`BluezMediaItem.getCoverArt()`).
- Relocated compiled consumer integration tests verifying native asset resolution.
- Additional CI test suites for private D-Bus daemon resynchronization and isolate shutdown.

### Fixed
- **State & Concurrency**:
  - Prevented delayed asynchronous property refreshes from overwriting newer proxy states.
  - Replayed concurrent updates after D-Bus daemon resynchronization.
  - Retried property invalidations superseded by fast-arriving D-Bus property events.
  - Discarded late updates and rejected calls on disposed media proxies (`BluezMediaPlayer`, `BluezMediaControl`, `BluezMediaFolder`, `BluezMediaItem`, `BluezMediaTransport`).
  - Kept proxy objects and Flutter example UI synchronized across refreshes, object lifecycle events, widget disposal, and overlapping requests.
- **Resource Management & Native Lifecycles**:
  - Automatically reclaimed native client instances and handles on Dart isolate shutdown.
  - Contained exceptions and handle validation inside descriptor C ABI entry points.
  - Ensured native client teardown completes cleanly during asynchronous `close()` operations.
  - Retained transport file descriptors and transport owners until explicit cleanup by Dart code.
  - Cleaned up timed-out cover-art downloads and temporary directories on client exit.
  - Revalidated stale OBEX sessions and rejected incomplete fast-finished transfers.
- **D-Bus & BlueZ Integration**:
  - Preserved original D-Bus error details when client creation fails.
  - Maintained D-Bus event loop responsiveness during remote operation calls.
  - Invalidated cached media states and resynchronized automatically when BlueZ service ownership changes.
  - Subscribed to property change signals prior to ObjectManager discovery to prevent missing early updates.
  - Serialized synchronous and raw asynchronous player registrations.
  - Supplied required MPRIS properties during local player registration and stopped advertising unimplemented local playback capabilities.
- **Build & Packaging**:
  - Fixed bundled library resolution via native asset ID mapping.
  - Optimized CMake target rules to compile native implementation objects only once.
  - Validated native build targets and tracked vendored input files in build hooks.

### Refactored
- Streamlined cover art loading and retry logic in the Flutter demo application.
- Kept unchecked native decoding routines out of production builds.
- Protected decoded media property snapshots against inadvertent caller mutation.

## [0.2.0] - 2026-08-20

### Added
- Experimental BlueZ OBEX BIP cover art retrieval service (`CoverArtService`).
- Exposed `ObexPort` property (uint16) on `MediaPlayer1` interface and FFI bindings.
- Automatic native-format cover art download support.
- New C ABI functions for cover art acquisition and native buffer memory management.
- Unit and lifetime tests for handle safety and cover art property decoding.

### Changed
- Refactored native object manager and client lifetime management.
- Updated Dart `BluezMediaPlayer` to include `getCoverArt()` and `obexPort` metadata access.

## [0.1.3] - 2026-08-13

### Added
- Implementation of `FastForward` and `Rewind` methods for `MediaPlayer1`.
- Added fast-forward and rewind controls in example applications.

### Changed
- Aligned native C ABI definitions across native layer and Dart FFI.
- Refactored property handling and string/value encoding in media proxy classes.

## [0.1.2] - 2026-06-12

### Added
- Integrated `MediaTransport1` proxies with automatic device-based media object grouping.
- Implemented `Repeat` and `Shuffle` properties for `MediaPlayer1`.
- Introduced `BluezMediaStatusCode` enum for native error reporting.
- Asynchronous D-Bus `ObjectManager` event loop handling.

### Changed
- Documented native status codes, full API surface, and troubleshooting steps in `README.md`.

## [0.1.1] - 2026-06-04

### Added
- Implementation of `MediaTransport1` streaming and volume control APIs.
- Media endpoint and transport registration setup.
- Example CLI scripts for transport status inspection and volume adjustment.

## [0.1.0] - 2026-06-03

### Added
- Core `BluezMediaClient` lifecycle management and connection initialization.
- Support for registering local `MediaPlayer1` instances via `org.bluez.Media1`.
- Remote `org.bluez.MediaPlayer1` playback controls (`Play`, `Pause`, `Stop`, `Next`, `Previous`, `Seek`).
- Remote `org.bluez.MediaControl1` command interface and properties.
- AVRCP browsing support for `org.bluez.MediaFolder1` and `org.bluez.MediaItem1`.
