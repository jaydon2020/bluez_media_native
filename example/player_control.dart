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

  try {
    switch (command) {
      case 'play':
        client.play(playerPath);
        break;
      case 'pause':
        client.pause(playerPath);
        break;
      case 'stop':
        client.stop(playerPath);
        break;
      case 'next':
        client.next(playerPath);
        break;
      case 'previous':
        client.previous(playerPath);
        break;
      case 'props':
        final props = client.getPlayerProperties(playerPath);
        print('Player: ${props.objectPath}');
        print('  Status:   ${props.status}');
        print('  Position: ${props.position} ms');
        print('  Name:     ${props.name}');
        print('  Type:     ${props.type}');
        print('  Device:   ${props.device}');
        print('  Track:');
        printProperties(props.track, indent: '    ');
        return;
      default:
        throw FormatException('Unknown command: $command.');
    }
    print('Sent $command to $playerPath.');
  } finally {
    client.close();
  }
}
