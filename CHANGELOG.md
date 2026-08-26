# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Existing-session-only cover art API for reusing sessions such as the one
  owned by `mpris-proxy` while leaving file caching to the caller.

### Fixed
- Revalidate stale OBEX sessions, reject partial fast-finished transfers, and
  contain native exceptions and acquired file descriptors at the Dart FFI boundary.
- Keep Dart proxy and Flutter example state synchronized across refreshes,
  object lifecycle events, widget disposal, and overlapping cover art requests.

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
