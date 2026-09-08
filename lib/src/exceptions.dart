/// Base exception for BlueZ media operations.
class BlueZMediaException implements Exception {
  final String message;

  const BlueZMediaException(this.message);

  @override
  String toString() => 'BlueZMediaException: $message';
}

/// Thrown when a D-Bus media operation fails.
class BlueZMediaOperationException extends BlueZMediaException {
  final String name;
  final String objectPath;

  const BlueZMediaOperationException(
    super.message, {
    required this.name,
    this.objectPath = '',
  });

  @override
  String toString() => 'BlueZMediaOperationException($name): $message';
}

/// Thrown when connecting to BlueZ or loading its initial snapshot fails.
class BlueZMediaServiceUnavailableException
    extends BlueZMediaOperationException {
  const BlueZMediaServiceUnavailableException([
    String message = 'BlueZ service is not available',
  ]) : super(message, name: 'org.freedesktop.DBus.Error.ServiceUnknown');

  const BlueZMediaServiceUnavailableException.withDetails(
    super.message, {
    required super.name,
    super.objectPath,
  });
}
