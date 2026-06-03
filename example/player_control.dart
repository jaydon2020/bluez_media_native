// ignore_for_file: avoid_print

// example/player_control.dart — control a remote org.bluez.MediaPlayer1.

import 'media_example_utils.dart';

void main(List<String> args) {
  if (args.length < 2 || hasFlag(args, '--help')) {
    printUsage('dart run example/player_control.dart <player_path> <command>', [
      'Commands: play, pause, stop, next, previous, props',
      '',
      'Example:',
      '  dart run example/player_control.dart '
          '/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0 play',
    ]);
    return;
  }

  final playerPath = args[0];
  final command = args[1].toLowerCase();
  final client = createClient();
  final player = client.player(playerPath);

  try {
    switch (command) {
      case 'play':
        player.play();
        break;
      case 'pause':
        player.pause();
        break;
      case 'stop':
        player.stop();
        break;
      case 'next':
        player.next();
        break;
      case 'previous':
        player.previous();
        break;
      case 'props':
        player.refresh();
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
    client.close();
  }
}
