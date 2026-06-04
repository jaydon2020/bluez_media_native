// ignore_for_file: avoid_print

// example/transport_control.dart — control a remote org.bluez.MediaTransport1.

import 'media_example_utils.dart';

void main(List<String> args) {
  if (args.length < 2 || hasFlag(args, '--help')) {
    printUsage('dart run example/transport_control.dart <transport_path> <command> [args...]', [
      'Commands:',
      '  props                   - Fetch and print transport properties',
      '  acquire                 - Acquire transport (returns FD)',
      '  try_acquire             - Try acquire transport (returns FD)',
      '  release                 - Release transport',
      '  set_volume <volume>     - Set absolute volume (0-127)',
      '',
      'Example:',
      '  dart run example/transport_control.dart /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/fd0 set_volume 64',
    ]);
    return;
  }

  final transportPath = args[0];
  final command = args[1].toLowerCase();
  final client = createClient();
  final transport = client.transport(transportPath);

  try {
    switch (command) {
      case 'props':
        transport.refresh();
        print('Transport: ${transport.objectPath}');
        print('  Device: ${transport.device}');
        print('  UUID:   ${transport.uuid}');
        print('  Codec:  ${transport.codec}');
        print('  State:  ${transport.state}');
        print('  Delay:  ${transport.delay}');
        print('  Volume: ${transport.volume}');
        return;
      case 'acquire':
        final result = transport.acquire();
        print('Acquired transport.');
        print('  FD:        ${result.fd}');
        print('  Read MTU:  ${result.readMtu}');
        print('  Write MTU: ${result.writeMtu}');
        break;
      case 'try_acquire':
        final result = transport.tryAcquire();
        print('Tried to acquire transport.');
        print('  FD:        ${result.fd}');
        print('  Read MTU:  ${result.readMtu}');
        print('  Write MTU: ${result.writeMtu}');
        break;
      case 'release':
        transport.release();
        print('Released transport.');
        break;
      case 'set_volume':
        if (args.length < 3) {
          throw FormatException('set_volume requires a volume argument (0-127).');
        }
        final volume = int.parse(args[2]);
        transport.volume = volume;
        print('Set volume to $volume.');
        break;
      default:
        throw FormatException('Unknown command: $command.');
    }
  } finally {
    client.close();
  }
}
