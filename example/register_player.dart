// ignore_for_file: avoid_print

// example/register_player.dart — register this process as a BlueZ media player.

import 'dart:async';

import 'package:bluez_media_native/bluez_media_native.dart';

import 'media_example_utils.dart';

Future<void> main(List<String> args) async {
  if (isListCommand(args)) {
    final client = createClient();
    try {
      printManagedMediaObjects(client, interfaces: {'org.bluez.Media1'});
    } finally {
      client.close();
    }
    return;
  }

  if (hasFlag(args, '--help')) {
    printUsage('dart run example/register_player.dart list | [options]', [
      'list                   List adapters exposing org.bluez.Media1',
      '--adapter <path>       BlueZ adapter path, default $kDefaultAdapterPath',
      '--player <path>        Local object path, default $kDefaultLocalPlayerPath',
      '--name <name>          Player name shown to BlueZ',
      '--hold <seconds>       Keep registration alive, default 30',
    ]);
    return;
  }

  final adapterPath = optionValue(
    args,
    '--adapter',
    fallback: kDefaultAdapterPath,
  );
  final playerPath = optionValue(
    args,
    '--player',
    fallback: kDefaultLocalPlayerPath,
  );
  final name = optionValue(args, '--name', fallback: 'bluez_media_native');
  final holdSeconds =
      int.tryParse(optionValue(args, '--hold', fallback: '30')) ?? 30;

  final client = createClient();
  try {
    client.registerPlayer(
      BluezMediaPlayerRegistrationConfig(
        adapterPath: adapterPath,
        playerPath: playerPath,
        name: name,
        browsable: true,
        searchable: true,
      ),
    );

    print('Registered player $playerPath on $adapterPath.');
    print('Keeping registration alive for $holdSeconds seconds...');
    await Future<void>.delayed(Duration(seconds: holdSeconds));

    client.unregisterPlayer(adapterPath: adapterPath, playerPath: playerPath);
    print('Unregistered player.');
  } finally {
    client.close();
  }
}
