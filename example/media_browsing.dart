// ignore_for_file: avoid_print

// example/media_browsing.dart — browse AVRCP folders and media items.

import 'package:bluez_media_native/bluez_media_native.dart';

import 'media_example_utils.dart';

void main(List<String> args) {
  if (args.length < 2 || hasFlag(args, '--help')) {
    printUsage('dart run example/media_browsing.dart <path> <command>', [
      'Folder commands: folder-props, list, search <text>, cd <folder_path>',
      'Item commands:   item-props, play-item, add-now-playing',
      '',
      'Examples:',
      '  dart run example/media_browsing.dart '
          '/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0 list',
      '  dart run example/media_browsing.dart '
          '/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0 search Coltrane',
      '  dart run example/media_browsing.dart '
          '/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/player0/item0 play-item',
    ]);
    return;
  }

  final path = args[0];
  final command = args[1].toLowerCase();
  final client = createClient();

  try {
    switch (command) {
      case 'folder-props':
        final props = client.getMediaFolderProperties(path);
        print('Folder: ${props.objectPath}');
        print('  Name:           ${props.name}');
        print('  NumberOfItems:  ${props.numberOfItems}');
        break;
      case 'list':
        final result = client.listFolderItems(path);
        print('Folder: ${result.objectPath}');
        print('Items: ${result.items.length}');
        for (final item in result.items) {
          _printItemSummary(item);
        }
        break;
      case 'search':
        if (args.length < 3) {
          throw const FormatException('search requires a text value.');
        }
        final result = client.searchFolder(path, args.sublist(2).join(' '));
        print('Search result folder: ${result.objectPath}');
        break;
      case 'cd':
        if (args.length < 3) {
          throw const FormatException('cd requires a target folder path.');
        }
        client.changeFolder(path, args[2]);
        print('Changed $path to folder ${args[2]}.');
        break;
      case 'item-props':
        final props = client.getMediaItemProperties(path);
        _printItemDetails(props);
        break;
      case 'play-item':
        client.playItem(path);
        print('Sent Play to item $path.');
        break;
      case 'add-now-playing':
        client.addItemToNowPlaying(path);
        print('Added item $path to now playing.');
        break;
      default:
        throw FormatException('Unknown command: $command.');
    }
  } finally {
    client.close();
  }
}

void _printItemSummary(BlueZMediaItemProps item) {
  final playable = item.playable ? 'playable' : 'not playable';
  print('- ${item.name} [$playable]');
  print('  Path: ${item.objectPath}');
  print('  Type: ${item.type}');
  if (item.folderType.isNotEmpty) {
    print('  FolderType: ${item.folderType}');
  }
}

void _printItemDetails(BlueZMediaItemProps item) {
  _printItemSummary(item);
  print('  Player: ${item.player}');
  if (item.metadata.isNotEmpty) {
    print('  Metadata:');
    printProperties(item.metadata, indent: '    ');
  }
}
