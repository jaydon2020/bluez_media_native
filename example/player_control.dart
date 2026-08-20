// ignore_for_file: avoid_print

// example/player_control.dart — control a remote org.bluez.MediaPlayer1.

import 'dart:io';

import 'media_example_utils.dart';

Future<void> main(List<String> args) async {
  if (isListCommand(args)) {
    final client = await createClient();
    try {
      await printManagedMediaObjects(
        client,
        interfaces: {'org.bluez.MediaPlayer1'},
      );
    } finally {
      await client.close();
    }
    return;
  }

  if (args.length < 2 || hasFlag(args, '--help')) {
    printUsage('dart run example/player_control.dart <player_path> <command>', [
      'Commands: list, play, pause, stop, next, previous, fast-forward, rewind,',
      '          cover-art <target_file>, props',
      '',
      'Example:',
      '  dart run example/player_control.dart list',
      '  dart run example/player_control.dart '
          '/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0 play',
    ]);
    return;
  }

  final playerPath = args[0];
  final command = args[1].toLowerCase();
  final client = await createClient();
  final player = client.player(playerPath);

  try {
    switch (command) {
      case 'play':
        await player.play();
        break;
      case 'pause':
        await player.pause();
        break;
      case 'stop':
        await player.stop();
        break;
      case 'next':
        await player.next();
        break;
      case 'previous':
        await player.previous();
        break;
      case 'fast-forward':
        await player.fastForward();
        break;
      case 'rewind':
        await player.rewind();
        break;
      case 'cover-art':
        if (args.length < 3) {
          throw const FormatException('cover-art requires a target file.');
        }
        final target = File(args[2]).absolute.path;
        print('Saved cover art to ${await player.getCoverArt(target)}.');
        return;
      case 'props':
        await player.refresh();
        print('Player: ${player.objectPath}');
        print('  Status:   ${player.status}');
        print('  Position: ${player.position} ms');
        print('  Name:     ${player.name}');
        print('  Type:     ${player.type}');
        print('  Device:   ${player.device}');
        print('  Track:');
        printProperties(player.track, indent: '    ');
        return;
      default:
        throw FormatException('Unknown command: $command.');
    }
    print('Sent $command to $playerPath.');
  } finally {
    await client.close();
  }
}
