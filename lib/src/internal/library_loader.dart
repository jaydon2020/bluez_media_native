import 'dart:ffi';
import 'dart:io';
import 'package:ffi/ffi.dart';

const String _libraryName = 'libbluez_media_native.so';

@Native<Pointer<Char> Function()>(
  symbol: 'bluez_media_library_path',
  assetId: 'package:bluez_media_native/src/ffi/bluez_media_native_asset.dart',
)
external Pointer<Char> _nativeLibraryPath();

DynamicLibrary loadBluezMediaNative() {
  // 1. Check BLUEZ_MEDIA_LIB environment variable override
  final override = Platform.environment['BLUEZ_MEDIA_LIB'];
  if (override != null && override.isNotEmpty) {
    return DynamicLibrary.open(override);
  }

  final errors = <String>[];

  // 2. Try standard dynamic library load (system paths / rpath).
  // DynamicLibrary.open wraps dlerror() in an ArgumentError on Dart 3.x / Linux.
  try {
    return DynamicLibrary.open(_libraryName);
  } on ArgumentError catch (e) {
    errors.add('dlopen($_libraryName): $e');
  }

  // 3. Search /proc/self/maps for sibling directories of loaded libraries (e.g., libapp.so)
  final loadedLibrarySibling = _findSiblingOfLoadedLibrary(_libraryName);
  if (loadedLibrarySibling != null) {
    try {
      return DynamicLibrary.open(loadedLibrarySibling);
    } on ArgumentError catch (e) {
      errors.add('dlopen($loadedLibrarySibling): $e');
    }
  }

  // 4. Fall back to Native Assets _nativeLibraryPath() if available
  try {
    final path = _nativeLibraryPath();
    if (path != nullptr) {
      final pathStr = path.cast<Utf8>().toDartString();
      if (pathStr.isNotEmpty) {
        try {
          return DynamicLibrary.open(pathStr);
        } on ArgumentError catch (e) {
          errors.add('dlopen($pathStr from native asset): $e');
        }
      }
    }
  } catch (e) {
    // Catches TypeError (asset not linked) and any other resolution failure.
    errors.add(
      '@Native bluez_media_library_path resolution failed '
      '(asset may not be linked): $e',
    );
  }

  // 5. Try candidate directories relative to executable or current working directory
  final candidates = <String>[];
  final executableDirectory = File(Platform.resolvedExecutable).parent.path;
  candidates.addAll([
    '$executableDirectory/lib/$_libraryName',
    '$executableDirectory/$_libraryName',
    '${Directory.current.path}/build/native/$_libraryName',
    '${Directory.current.path}/build/$_libraryName',
  ]);

  for (final path in candidates) {
    final file = File(path);
    if (file.existsSync()) {
      try {
        return DynamicLibrary.open(file.absolute.path);
      } on ArgumentError catch (e) {
        errors.add('dlopen(${file.absolute.path}): $e');
      }
    }
  }

  throw StateError(
    'Failed to load $_libraryName. Set BLUEZ_MEDIA_LIB to its absolute path.\n'
    'Errors:\n${errors.join('\n')}',
  );
}

/// Scans `/proc/self/maps` (Linux only) for any already-loaded shared library
/// whose directory also contains [libraryName]. This handles Flutter/AGL
/// deployments where `libbluez_media_native.so` is co-located with `libapp.so`.
String? _findSiblingOfLoadedLibrary(String libraryName) {
  // /proc/self/maps only exists on Linux; bail out on other platforms.
  if (!Platform.isLinux) return null;
  try {
    final maps = File('/proc/self/maps');
    if (!maps.existsSync()) return null;
    final directories = <String>{};
    for (final line in maps.readAsLinesSync()) {
      final lastSpace = line.lastIndexOf(' ');
      if (lastSpace == -1) continue;
      final path = line.substring(lastSpace + 1).trim();
      if (!path.startsWith('/')) continue;
      final directory = File(path).parent.path;
      if (!directories.add(directory)) continue;
      final candidate = '$directory/$libraryName';
      if (File(candidate).existsSync()) return candidate;
    }
  } on Exception {
    // Swallow I/O errors; this is a best-effort probe.
    return null;
  }
  return null;
}
