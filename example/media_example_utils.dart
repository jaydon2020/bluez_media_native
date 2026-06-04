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

bool isListCommand(List<String> args) =>
    args.length == 1 && args.single.toLowerCase() == 'list';

void printManagedMediaObjects(
  BluezMediaClient client, {
  required Set<String> interfaces,
}) {
  final objects = client.getManagedObjects();
  final pathsByInterface = <String, List<String>>{
    'org.bluez.Media1': objects.media,
    'org.bluez.MediaPlayer1': objects.players,
    'org.bluez.MediaControl1': objects.controls,
    'org.bluez.MediaTransport1': objects.transports,
    'org.bluez.MediaFolder1': objects.folders,
    'org.bluez.MediaItem1': objects.items,
  };

  for (final entry in pathsByInterface.entries) {
    if (!interfaces.contains(entry.key)) continue;
    print('${entry.key}:');
    if (entry.value.isEmpty) {
      print('  (none)');
      continue;
    }
    for (final path in entry.value) {
      print('  $path');
    }
  }
}
