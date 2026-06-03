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
  final control = client.control(controlPath);

  try {
    switch (command) {
      case 'play':
        control.play();
        break;
      case 'pause':
        control.pause();
        break;
      case 'stop':
        control.stop();
        break;
      case 'next':
        control.next();
        break;
      case 'previous':
        control.previous();
        break;
      case 'volume-up':
        control.volumeUp();
        break;
      case 'volume-down':
        control.volumeDown();
        break;
      case 'fast-forward':
        control.fastForward();
        break;
      case 'rewind':
        control.rewind();
        break;
      case 'props':
        _printControlProperties(control);
        return;
      case 'watch':
        final seconds =
            int.tryParse(optionValue(args, '--seconds', fallback: '30')) ?? 30;
        for (var i = 0; i < seconds; i++) {
          _printControlProperties(control);
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

void _printControlProperties(BluezMediaControl control) {
  control.refresh();
  final timestamp = DateTime.now().toIso8601String().substring(11, 19);
  print('[$timestamp] Control: ${control.objectPath}');
  print('  Connected: ${control.connected}');
  print('  Player:    ${control.playerPath}');
}
