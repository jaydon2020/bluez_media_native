// ignore_for_file: avoid_print

import 'dart:io';

import 'package:bluez_media_native/bluez_media_native.dart';

import 'media_example_utils.dart';

Future<void> main(List<String> args) async {
  if (args.isEmpty || hasFlag(args, '--help')) {
    printUsage('dart run example/media_cover_art.dart <target_file>', [
      'Options:',
      '  --player <object_path>  Use a specific MediaPlayer1 object.',
      '  --native                Own an OBEX session instead of using MPRIS.',
      '',
      'MPRIS is the default. Use --native only when mpris-proxy is disabled.',
    ]);
    return;
  }

  final target = File(args.first).absolute;
  if (target.existsSync()) {
    throw ArgumentError('Target file already exists: ${target.path}');
  }

  final native = hasFlag(args, '--native');
  final requestedPlayer = hasFlag(args, '--player')
      ? optionValue(args, '--player')
      : null;
  final client = await BluezMediaClient.create(manageCoverArt: native);
  try {
    final player = requestedPlayer == null
        ? await _currentPlayer(client)
        : client.player(requestedPlayer);
    await player.refresh();

    if (native) {
      await player.getCoverArt(target.path);
    } else {
      final itemPath = _trackValue(player, 'Item');
      if (itemPath.isEmpty && player.imageHandle.isEmpty) {
        throw StateError('The current track has no cover-art metadata.');
      }
      List<int>? bytes;
      for (var attempt = 0; itemPath.isNotEmpty && attempt < 2; attempt++) {
        try {
          bytes = await client.getMprisCoverArt(itemPath);
          break;
        } catch (_) {
          await Future<void>.delayed(const Duration(seconds: 2));
        }
      }
      if (bytes != null) {
        await target.writeAsBytes(bytes, flush: true);
      } else {
        await player.getCoverArtFromExistingSession(target.path);
      }
    }
    print('Saved cover art to ${target.path}.');
  } finally {
    await client.close();
  }
}

Future<BluezMediaPlayer> _currentPlayer(BluezMediaClient client) async {
  for (final player in client.players) {
    if (player.status.toLowerCase() == 'playing') return player;
  }
  if (client.players.isNotEmpty) return client.players.first;
  return client.playerAdded.first.timeout(
    const Duration(seconds: 20),
    onTimeout: () => throw StateError(
      'No BlueZ MediaPlayer1 appeared within 20 seconds. Connect an AVRCP '
      'device and start media playback, then try again.',
    ),
  );
}

String _trackValue(BluezMediaPlayer player, String key) {
  for (final property in player.track) {
    if (property.key == key) return property.value;
  }
  return '';
}
