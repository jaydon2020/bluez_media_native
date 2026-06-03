// types.dart — Dart-side struct mirrors for glaze-decoded BlueZ Media payloads.
// These match the C++ structs in native/include/bluez_media_types.h.

/// A string representation of a D-Bus variant property.
class BlueZMediaProperty {
  final String key;
  final String value;

  const BlueZMediaProperty({required this.key, required this.value});
}

/// MediaPlayer1 properties from BlueZ.
class BlueZMediaPlayerProps {
  final String objectPath;
  final int changedMask;
  final String equalizer;
  final String repeat;
  final String shuffle;
  final String scan;
  final String status;
  final int position;
  final List<BlueZMediaProperty> track;
  final String device;
  final String name;
  final String type;
  final String subtype;
  final bool browsable;
  final bool searchable;
  final String playlist;

  const BlueZMediaPlayerProps({
    required this.objectPath,
    this.changedMask = 0,
    this.equalizer = '',
    this.repeat = '',
    this.shuffle = '',
    this.scan = '',
    this.status = '',
    this.position = 0,
    this.track = const [],
    this.device = '',
    this.name = '',
    this.type = '',
    this.subtype = '',
    this.browsable = false,
    this.searchable = false,
    this.playlist = '',
  });
}

/// MediaControl1 properties from BlueZ.
class BlueZMediaControlProps {
  final String objectPath;
  final int changedMask;
  final bool connected;
  final String player;

  const BlueZMediaControlProps({
    required this.objectPath,
    this.changedMask = 0,
    this.connected = false,
    this.player = '',
  });
}

/// MediaEndpoint1 properties and configuration from BlueZ.
class BlueZMediaEndpointProps {
  final String objectPath;
  final String uuid;
  final int codec;
  final List<int> capabilities;
  final String device;
  final bool delayReporting;

  const BlueZMediaEndpointProps({
    required this.objectPath,
    this.uuid = '',
    this.codec = 0,
    this.capabilities = const [],
    this.device = '',
    this.delayReporting = false,
  });
}

/// MediaTransport1 properties from BlueZ.
class BlueZMediaTransportProps {
  final String objectPath;
  final String device;
  final String uuid;
  final int codec;
  final List<int> configuration;
  final String state;
  final int delay;
  final int volume;
  final String endpoint;

  const BlueZMediaTransportProps({
    required this.objectPath,
    this.device = '',
    this.uuid = '',
    this.codec = 0,
    this.configuration = const [],
    this.state = '',
    this.delay = 0,
    this.volume = 0,
    this.endpoint = '',
  });
}

/// MediaFolder1 properties from BlueZ.
class BlueZMediaFolderProps {
  final String objectPath;
  final int numberOfItems;
  final String name;

  const BlueZMediaFolderProps({
    required this.objectPath,
    this.numberOfItems = 0,
    this.name = '',
  });
}

/// MediaItem1 properties from BlueZ.
class BlueZMediaItemProps {
  final String objectPath;
  final String player;
  final String name;
  final String type;
  final String folderType;
  final bool playable;
  final List<BlueZMediaProperty> metadata;

  const BlueZMediaItemProps({
    required this.objectPath,
    this.player = '',
    this.name = '',
    this.type = '',
    this.folderType = '',
    this.playable = false,
    this.metadata = const [],
  });
}

/// Result from MediaTransport1.Acquire / TryAcquire.
class BlueZMediaAcquireResult {
  final String transportPath;
  final int fd;
  final int readMtu;
  final int writeMtu;

  const BlueZMediaAcquireResult({
    required this.transportPath,
    required this.fd,
    required this.readMtu,
    required this.writeMtu,
  });
}

/// D-Bus error from a failed BlueZ Media method call.
class BlueZMediaError {
  final String objectPath;
  final String name;
  final String message;

  const BlueZMediaError({
    required this.objectPath,
    required this.name,
    required this.message,
  });
}
