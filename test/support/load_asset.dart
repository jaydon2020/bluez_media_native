import 'dart:io';
import 'package:bluez_media_native/bluez_media_native_bindings_generated.dart';
import 'package:bluez_media_native/src/internal/library_loader.dart';

void main() {
  Directory.current = Directory.systemTemp;
  final bindings = BluezMediaNativeBindings(loadBluezMediaNative());
  if (bindings.bluez_media_close_fd(-1) != -1) {
    throw StateError('Native asset did not return the expected ABI status');
  }
  print('Native asset resolved outside the checkout.');
}
