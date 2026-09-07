import 'dart:isolate';
import 'package:bluez_media_native/bluez_media_native.dart';

Future<void> main(List<String> args, SendPort ready) async {
  final client = await BluezMediaClient.create();
  ready.send(true);
  ReceivePort().listen((_) => client.players);
}
