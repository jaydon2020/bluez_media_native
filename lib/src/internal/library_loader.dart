import 'dart:ffi';
import 'dart:io';
import 'package:ffi/ffi.dart';

// Resolve this anchor through the asset ID emitted by hook/build.dart. The raw
// generated bindings can then keep their existing DynamicLibrary interface.
@Native<Pointer<Char> Function()>(
  symbol: 'bluez_media_library_path',
  assetId: 'package:bluez_media_native/src/ffi/bluez_media_native_asset.dart',
)
external Pointer<Char> _nativeLibraryPath();

DynamicLibrary loadBluezMediaNative() {
  final override = Platform.environment['BLUEZ_MEDIA_LIB'];
  if (override != null && override.isNotEmpty) {
    return DynamicLibrary.open(override);
  }
  final path = _nativeLibraryPath();
  if (path == nullptr) {
    throw StateError('Cannot resolve the BlueZ native asset.');
  }
  return DynamicLibrary.open(path.cast<Utf8>().toDartString());
}
