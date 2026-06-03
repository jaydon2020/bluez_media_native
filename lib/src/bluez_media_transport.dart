import 'package:meta/meta.dart';

import 'bluez_media_client.dart';

/// A proxy for a remote `org.bluez.MediaTransport1` object.
class BluezMediaTransport {
  final BluezMediaClient _client;
  final String objectPath;
  BlueZMediaTransportProps? _props;

  /// Do not instantiate directly. Use [BluezMediaClient.transport].
  @internal
  BluezMediaTransport.internal(this._client, this.objectPath);

  /// Synchronous snapshot of the transport properties.
  /// Call [refresh] first to update this snapshot from BlueZ.
  BlueZMediaTransportProps get props {
    if (_props == null) {
      refresh();
    }
    return _props!;
  }

  /// The UUID of the profile that the transport is for.
  String get uuid => props.uuid;

  /// The codec of the transport.
  int get codec => props.codec;

  /// The configuration of the transport.
  List<int> get configuration => props.configuration;

  /// The state of the transport.
  String get state => props.state;

  /// The delay of the transport in 1/10 of millisecond.
  int get delay => props.delay;

  /// The endpoint associated with this transport.
  String get endpoint => props.endpoint;

  /// The device associated with this transport.
  String get device => props.device;

  /// The volume of the transport.
  int get volume => props.volume;

  /// Sets the volume of the transport.
  /// Automatically refreshes the property snapshot after the update.
  set volume(int value) {
    _client.transportSetVolume(objectPath, value);
    refresh();
  }

  /// Acquires the transport file descriptor.
  BlueZMediaAcquireResult acquire() {
    return _client.transportAcquire(objectPath);
  }

  /// Tries to acquire the transport file descriptor without blocking.
  BlueZMediaAcquireResult tryAcquire() {
    return _client.transportTryAcquire(objectPath);
  }

  /// Releases the transport file descriptor.
  void release() {
    _client.transportRelease(objectPath);
  }

  /// Fetches the latest properties from BlueZ and updates the snapshot.
  void refresh() {
    _props = _client.getMediaTransportProperties(objectPath);
  }

  @internal
  void updateProps(BlueZMediaTransportProps props) {
    _props = props;
  }
}
