/// Base exception for BlueZ media operations.
class BlueZMediaException implements Exception {
  final String message;

  const BlueZMediaException(this.message);

  @override
  String toString() => 'BlueZMediaException: $message';
}

/// Thrown when BlueZ is unavailable on the system bus.
class BlueZMediaServiceUnavailableException extends BlueZMediaException {
  const BlueZMediaServiceUnavailableException([
    super.message = 'BlueZ service is not available',
  ]);
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
