// ignore_for_file: avoid_print

// example/media_browsing.dart — browse AVRCP folders and media items.

import 'package:bluez_media_native/bluez_media_native.dart';

import 'media_example_utils.dart';

void main(List<String> args) {
  if (isListCommand(args)) {
    final client = createClient();
    try {
      printManagedMediaObjects(
        client,
        interfaces: {
          'org.bluez.MediaPlayer1',
          'org.bluez.MediaFolder1',
          'org.bluez.MediaItem1',
        },
      );
    } finally {
      client.close();
    }
    return;
  }

  if (args.length < 2 || hasFlag(args, '--help')) {
    printUsage('dart run example/media_browsing.dart <path> <command>', [
      'Player commands: play, pause, stop, next, previous, player-props',
      'Folder commands: folder-props, list, search <text>, cd <folder_path>',
      'Item commands:   item-props, play-item, add-now-playing',
      'Discovery:       list',
      '',
      'Examples:',
      '  dart run example/media_browsing.dart list',
      '  dart run example/media_browsing.dart '
          '/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0 list',
      '  dart run example/media_browsing.dart '
          '/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0 search Coltrane',
      '  dart run example/media_browsing.dart '
          '/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/avrcp/player0/item0 play-item',
    ]);
    return;
  }

  final path = args[0];
  final command = args[1].toLowerCase();
  final client = createClient();
  final player = client.player(path);
  final folder = client.folder(path);
  final item = client.item(path);

  try {
    switch (command) {
      case 'play':
        player.play();
        print('Sent Play to player $path.');
        break;
      case 'pause':
        player.pause();
        print('Sent Pause to player $path.');
        break;
      case 'stop':
        player.stop();
        print('Sent Stop to player $path.');
        break;
      case 'next':
        player.next();
        print('Sent Next to player $path.');
        break;
      case 'previous':
        player.previous();
        print('Sent Previous to player $path.');
        break;
      case 'player-props':
        player.refresh();
        print('Player: ${player.objectPath}');
        print('  Status:   ${player.status}');
        print('  Position: ${player.position} ms');
        print('  Name:     ${player.name}');
        print('  Type:     ${player.type}');
        print('  Device:   ${player.device}');
        print('  Track:');
        printProperties(player.track, indent: '    ');
        break;
      case 'folder-props':
        folder.refresh();
        print('Folder: ${folder.objectPath}');
        print('  Name:           ${folder.name}');
        print('  NumberOfItems:  ${folder.numberOfItems}');
        break;
      case 'list':
        final items = folder.listItems();
        print('Folder: ${folder.objectPath}');
        print('Items: ${items.length}');
        for (final item in items) {
          _printItemSummary(item);
        }
        break;
      case 'search':
        if (args.length < 3) {
          throw const FormatException('search requires a text value.');
        }
        final result = folder.search(args.sublist(2).join(' '));
        print('Search result folder: ${result.objectPath}');
        break;
      case 'cd':
        if (args.length < 3) {
          throw const FormatException('cd requires a target folder path.');
        }
        folder.changeFolderPath(args[2]);
        print('Changed $path to folder ${args[2]}.');
        break;
      case 'item-props':
        item.refresh();
        _printItemDetails(item);
        break;
      case 'play-item':
        item.play();
        print('Sent Play to item $path.');
        break;
      case 'add-now-playing':
        item.addToNowPlaying();
        print('Added item $path to now playing.');
        break;
      default:
        throw FormatException('Unknown command: $command.');
    }
  } finally {
    client.close();
  }
}

void _printItemSummary(BluezMediaItem item) {
  final playable = item.playable ? 'playable' : 'not playable';
  print('- ${item.name} [$playable]');
  print('  Path: ${item.objectPath}');
  print('  Type: ${item.type}');
  if (item.folderType.isNotEmpty) {
    print('  FolderType: ${item.folderType}');
  }
}

void _printItemDetails(BluezMediaItem item) {
  _printItemSummary(item);
  print('  Player: ${item.playerPath}');
  if (item.metadata.isNotEmpty) {
    print('  Metadata:');
    printProperties(item.metadata, indent: '    ');
  }
}
