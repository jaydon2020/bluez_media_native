// ignore_for_file: avoid_print

// example/media_example_utils.dart — shared helpers for CLI examples.

import 'package:bluez_media_native/bluez_media_native.dart';

const String kDefaultAdapterPath = '/org/bluez/hci0';
const String kDefaultLocalPlayerPath = '/bluez_media/player0';

String optionValue(List<String> args, String name, {String? fallback}) {
  final index = args.indexOf(name);
  if (index == -1 || index + 1 >= args.length) {
    if (fallback != null) {
      return fallback;
    }
    throw FormatException('Missing value for $name.');
  }
  return args[index + 1];
}

bool hasFlag(List<String> args, String name) => args.contains(name);

void printUsage(String usage, List<String> details) {
  print('Usage: $usage');
  if (details.isEmpty) {
    return;
  }
  print('');
  for (final detail in details) {
    print(detail);
  }
}

BluezMediaClient createClient() {
  try {
    return BluezMediaClient.create();
  } on StateError catch (error) {
    throw StateError(
      '$error\n'
      'Make sure BlueZ is running and the native library is available. '
      'From the plugin root, run through Flutter tooling or build the native '
      'library before running CLI examples.',
    );
  }
}

void printProperties(
  List<BlueZMediaProperty> properties, {
  String indent = '',
}) {
  for (final property in properties) {
    print('$indent${property.key}: ${property.value}');
  }
}
