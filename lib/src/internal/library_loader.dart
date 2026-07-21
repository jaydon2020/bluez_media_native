import 'dart:ffi';
import 'dart:io';

const String _libraryName = 'libbluez_media_native.so';

DynamicLibrary loadBluezMediaNative() {
  final override = Platform.environment['BLUEZ_MEDIA_LIB'];
  if (override != null && override.isNotEmpty) {
    return DynamicLibrary.open(override);
  }

  final errors = <String>[];
  try {
    return DynamicLibrary.open(_libraryName);
  } on ArgumentError catch (error) {
    errors.add('dlopen($_libraryName): $error');
  }

  final loadedLibrarySibling = _findSiblingOfLoadedLibrary(_libraryName);
  if (loadedLibrarySibling != null) {
    try {
      return DynamicLibrary.open(loadedLibrarySibling);
    } on ArgumentError catch (error) {
      errors.add('$loadedLibrarySibling: $error');
    }
  }

  final candidates = <String>[];
  final hookArtifact = _findHookArtifact(_libraryName);
  if (hookArtifact != null) candidates.add(hookArtifact);

  try {
    final scriptDirectory = File(Platform.script.toFilePath()).parent.path;
    candidates.addAll([
      '$scriptDirectory/lib/$_libraryName',
      '$scriptDirectory/../lib/$_libraryName',
      '$scriptDirectory/../../lib/$_libraryName',
    ]);
  } on Exception {
    // Platform.script need not be a file URI.
  }

  final executableDirectory = File(Platform.resolvedExecutable).parent.path;
  candidates.addAll([
    '$executableDirectory/lib/$_libraryName',
    '$executableDirectory/$_libraryName',
    '${Directory.current.path}/build/native/$_libraryName',
    '${Directory.current.path}/build/$_libraryName',
  ]);

  for (final directory in (Platform.environment['LD_LIBRARY_PATH'] ?? '').split(
    ':',
  )) {
    if (directory.isNotEmpty) candidates.add('$directory/$_libraryName');
  }

  for (final path in candidates) {
    final file = File(path);
    if (!file.existsSync()) continue;
    try {
      return DynamicLibrary.open(file.absolute.path);
    } on ArgumentError catch (error) {
      errors.add('${file.absolute.path}: $error');
    }
  }

  throw StateError(
    'Failed to load $_libraryName. Set BLUEZ_MEDIA_LIB to its absolute path.\n'
    'Candidates:\n${candidates.map((path) => '  $path').join('\n')}\n'
    'Errors:\n${errors.join('\n')}',
  );
}

String? _findHookArtifact(String libraryName) {
  var directory = Directory.current;
  for (var depth = 0; depth < 6; depth++) {
    final root = Directory(
      '${directory.path}/.dart_tool/hooks_runner/shared/'
      'bluez_media_native/build',
    );
    if (root.existsSync()) {
      File? newest;
      var newestTime = DateTime.fromMillisecondsSinceEpoch(0);
      for (final entity in root.listSync(recursive: true)) {
        if (entity is File && entity.path.endsWith('/$libraryName')) {
          final modified = entity.statSync().modified;
          if (modified.isAfter(newestTime)) {
            newest = entity;
            newestTime = modified;
          }
        }
      }
      if (newest != null) return newest.path;
    }
    if (directory.parent.path == directory.path) break;
    directory = directory.parent;
  }
  return null;
}

String? _findSiblingOfLoadedLibrary(String libraryName) {
  try {
    final maps = File('/proc/self/maps');
    if (!maps.existsSync()) return null;
    final directories = <String>{};
    for (final line in maps.readAsLinesSync()) {
      final path = line.substring(line.lastIndexOf(' ') + 1);
      if (!path.startsWith('/')) continue;
      final directory = File(path).parent.path;
      if (!directories.add(directory)) continue;
      final candidate = '$directory/$libraryName';
      if (File(candidate).existsSync()) return candidate;
    }
  } on FileSystemException {
    return null;
  }
  return null;
}
