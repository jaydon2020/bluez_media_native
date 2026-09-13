import 'dart:isolate';
import 'package:bluez_media_native/bluez_media_native.dart';

Future<void> main(List<String> args, SendPort ready) async {
  final client = await BluezMediaClient.create(
    coverArtMode: BluezMediaCoverArtMode.disabled,
  );
  final acquired = args.contains('acquire')
      ? await client.transport('/transport').acquire()
      : null;
  ready.send(acquired?.fd ?? true);
  ReceivePort().listen((_) {
    client.players;
    acquired?.fd;
  });
}
