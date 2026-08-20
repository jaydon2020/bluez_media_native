// ignore_for_file: avoid_print

// example/transport_control.dart — control a remote org.bluez.MediaTransport1.

import 'media_example_utils.dart';

Future<void> main(List<String> args) async {
  if (args.isEmpty || hasFlag(args, '--help')) {
    printUsage('dart run example/transport_control.dart <command> [args...]', [
      'Commands:',
      '  list                    - List all active transport objects',
      '  props <path>            - Fetch and print transport properties',
      '  acquire <path>          - Acquire transport (returns FD)',
      '  try_acquire <path>      - Try acquire transport (returns FD)',
      '  release <path>          - Release transport',
      '  set_volume <path> <vol> - Set absolute volume (0-127)',
      '',
      'Example:',
      '  dart run example/transport_control.dart list',
      '  dart run example/transport_control.dart props /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/sep4/fd0',
    ]);
    return;
  }

  final command = args[0].toLowerCase();

  if (command == 'list') {
    final client = await createClient();
    try {
      await printManagedMediaObjects(
        client,
        interfaces: {'org.bluez.MediaTransport1'},
      );
    } finally {
      await client.close();
    }
    return;
  }

  if (args.length < 2) {
    print('Error: Command "$command" requires a transport path.');
    return;
  }

  final transportPath = args[1];
  final client = await createClient();
  final transport = client.transport(transportPath);

  try {
    switch (command) {
      case 'props':
        await transport.refresh();
        print('Transport: ${transport.objectPath}');
        print('  Device: ${transport.device}');
        print('  UUID:   ${transport.uuid}');
        print('  Codec:  ${transport.codec}');
        print('  State:  ${transport.state}');
        print('  Delay:  ${transport.delay}');
        print('  Volume: ${transport.volume}');
        return;
      case 'acquire':
        final result = await transport.acquire();
        try {
          print('Acquired transport.');
          print('  FD:        ${result.fd}');
          print('  Read MTU:  ${result.readMtu}');
          print('  Write MTU: ${result.writeMtu}');
        } finally {
          result.close();
        }
        break;
      case 'try_acquire':
        final result = await transport.tryAcquire();
        try {
          print('Tried to acquire transport.');
          print('  FD:        ${result.fd}');
          print('  Read MTU:  ${result.readMtu}');
          print('  Write MTU: ${result.writeMtu}');
        } finally {
          result.close();
        }
        break;
      case 'release':
        await transport.release();
        print('Released transport.');
        break;
      case 'set_volume':
        if (args.length < 3) {
          throw const FormatException(
            'set_volume requires a volume argument (0-127).',
          );
        }
        final volume = int.parse(args[2]);
        await transport.setVolume(volume);
        print('Set volume to $volume.');
        break;
      default:
        throw FormatException('Unknown command: $command.');
    }
  } finally {
    await client.close();
  }
}
