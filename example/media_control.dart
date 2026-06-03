// ignore_for_file: avoid_print

// example/media_control.dart — use deprecated org.bluez.MediaControl1 hooks.

import 'dart:async';

import 'package:bluez_media_native/bluez_media_native.dart';

import 'media_example_utils.dart';

Future<void> main(List<String> args) async {
  if (args.length < 2 || hasFlag(args, '--help')) {
    printUsage('dart run example/media_control.dart <control_path> <command>', [
      'Commands: play, pause, stop, next, previous, volume-up, volume-down,',
      '          fast-forward, rewind, props, watch',
      '',
      'Example:',
      '  dart run example/media_control.dart '
          '/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF volume-up',
    ]);
    return;
  }

  final controlPath = args[0];
  final command = args[1].toLowerCase();
  final client = createClient();

  try {
    switch (command) {
      case 'play':
        client.controlPlay(controlPath);
        break;
      case 'pause':
        client.controlPause(controlPath);
        break;
      case 'stop':
        client.controlStop(controlPath);
        break;
      case 'next':
        client.controlNext(controlPath);
        break;
      case 'previous':
        client.controlPrevious(controlPath);
        break;
      case 'volume-up':
        client.volumeUp(controlPath);
        break;
      case 'volume-down':
        client.volumeDown(controlPath);
        break;
      case 'fast-forward':
        client.fastForward(controlPath);
        break;
      case 'rewind':
        client.rewind(controlPath);
        break;
      case 'props':
        _printControlProperties(client, controlPath);
        return;
      case 'watch':
        final seconds =
            int.tryParse(optionValue(args, '--seconds', fallback: '30')) ?? 30;
        for (var i = 0; i < seconds; i++) {
          _printControlProperties(client, controlPath);
          await Future<void>.delayed(const Duration(seconds: 1));
        }
        return;
      default:
        throw FormatException('Unknown command: $command.');
    }
    print('Sent $command to $controlPath.');
  } finally {
    client.close();
  }
}

void _printControlProperties(BluezMediaClient client, String controlPath) {
  final props = client.getMediaControlProperties(controlPath);
  final timestamp = DateTime.now().toIso8601String().substring(11, 19);
  print('[$timestamp] Control: ${props.objectPath}');
  print('  Connected: ${props.connected}');
  print('  Player:    ${props.player}');
}
