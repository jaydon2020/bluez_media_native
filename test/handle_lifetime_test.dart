// SPDX-License-Identifier: Apache-2.0
//
// Handle lifetime.
//
// Native entry points receive an opaque token from Dart. Unknown and retired
// tokens must be rejected before native state is accessed. Reaching the end
// of these tests is part of the assertion: a stale-pointer dereference would
// terminate the process instead of producing a Dart failure.

import 'dart:ffi';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:bluez_media_native/bluez_media_native_bindings_generated.dart';
import 'package:bluez_media_native/src/ffi/codec.dart';
import 'package:bluez_media_native/src/ffi/types.dart';
import 'package:bluez_media_native/src/internal/library_loader.dart';
import 'package:ffi/ffi.dart';
import 'package:test/test.dart';

void main() {
  late BluezMediaNativeBindings bindings;

  setUpAll(() {
    bindings = BluezMediaNativeBindings(loadBluezMediaNative());
    bindings.bluez_media_init(NativeApi.initializeApiDLData);
  });

  final bogus = <Pointer<Void>>[
    nullptr,
    Pointer<Void>.fromAddress(1 << 40),
    Pointer<Void>.fromAddress(0xdeadbeef),
  ];

  test('handles the registry never issued are refused', () {
    for (final handle in bogus) {
      _expectHandleRefused(bindings, handle);
    }
  });

  test('destroying unknown handles is harmless', () {
    for (final handle in bogus) {
      bindings.bluez_media_client_destroy(handle);
    }
  });

  test(
    'an asynchronous call reports a typed error for unknown handles',
    () async {
      for (final handle in bogus) {
        final resultPort = ReceivePort();
        bindings.bluez_media_call_async(
          handle,
          BLUEZ_MEDIA_OP_PLAYER_PLAY,
          nullptr.cast(),
          nullptr.cast(),
          0,
          resultPort.sendPort.nativePort,
        );
        final message = await resultPort.first.timeout(
          const Duration(seconds: 2),
        );
        resultPort.close();

        expect(message, isA<Uint8List>());
        final bytes = message as Uint8List;
        expect(bytes.first, 0x20);
        final error = GlazeCodec.decode<BlueZMediaError>(bytes, 1);
        expect(error.name, 'org.bluez.Error.InvalidArguments');
      }
    },
  );

  test('a retired handle is refused', () {
    final handle = bindings.bluez_media_client_create(0);
    if (handle == nullptr) {
      return; // No BlueZ service is available in this test environment.
    }

    bindings.bluez_media_client_destroy(handle);
    _expectHandleRefused(bindings, handle);
    bindings.bluez_media_client_destroy(handle); // Double destroy.
  });
}

void _expectHandleRefused(
  BluezMediaNativeBindings bindings,
  Pointer<Void> handle,
) {
  final adapter = '/org/bluez/hci0'.toNativeUtf8();
  final player = '/org/bluez/hci0/dev_00/player0'.toNativeUtf8();
  final control = '/org/bluez/hci0/dev_00'.toNativeUtf8();
  final folder = '/org/bluez/hci0/dev_00/player0'.toNativeUtf8();
  final item = '/org/bluez/hci0/dev_00/player0/item0'.toNativeUtf8();
  final transport = '/org/bluez/hci0/dev_00/fd0'.toNativeUtf8();
  final target = '/tmp/bluez-media-handle-lifetime'.toNativeUtf8();
  final value = 'off'.toNativeUtf8();
  final registration = calloc<BluezMediaPlayerRegistration>();
  final output = calloc<BluezMediaBuffer>();
  registration.ref
    ..adapter_path = adapter.cast()
    ..player_path = player.cast()
    ..name = value.cast()
    ..type = value.cast()
    ..subtype = value.cast();

  final invalid = BluezMediaStatusCode.BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT.value;
  void refused(int result) => expect(result, invalid);

  try {
    refused(bindings.bluez_media_register_player(handle, registration));
    refused(
      bindings.bluez_media_unregister_player(
        handle,
        adapter.cast(),
        player.cast(),
      ),
    );

    refused(bindings.bluez_media_player_play(handle, player.cast()));
    refused(bindings.bluez_media_player_pause(handle, player.cast()));
    refused(bindings.bluez_media_player_stop(handle, player.cast()));
    refused(bindings.bluez_media_player_next(handle, player.cast()));
    refused(bindings.bluez_media_player_previous(handle, player.cast()));
    refused(bindings.bluez_media_player_fast_forward(handle, player.cast()));
    refused(bindings.bluez_media_player_rewind(handle, player.cast()));
    refused(
      bindings.bluez_media_player_set_repeat(
        handle,
        player.cast(),
        value.cast(),
      ),
    );
    refused(
      bindings.bluez_media_player_set_shuffle(
        handle,
        player.cast(),
        value.cast(),
      ),
    );
    refused(
      bindings.bluez_media_player_get_properties(handle, player.cast(), output),
    );
    refused(
      bindings.bluez_media_player_get_cover_art(
        handle,
        player.cast(),
        target.cast(),
        1,
      ),
    );
    refused(
      bindings.bluez_media_player_get_cover_art_from_existing_session(
        handle,
        player.cast(),
        target.cast(),
        1,
      ),
    );

    refused(bindings.bluez_media_control_play(handle, control.cast()));
    refused(bindings.bluez_media_control_pause(handle, control.cast()));
    refused(bindings.bluez_media_control_stop(handle, control.cast()));
    refused(bindings.bluez_media_control_next(handle, control.cast()));
    refused(bindings.bluez_media_control_previous(handle, control.cast()));
    refused(bindings.bluez_media_control_volume_up(handle, control.cast()));
    refused(bindings.bluez_media_control_volume_down(handle, control.cast()));
    refused(bindings.bluez_media_control_fast_forward(handle, control.cast()));
    refused(bindings.bluez_media_control_rewind(handle, control.cast()));
    refused(
      bindings.bluez_media_control_get_properties(
        handle,
        control.cast(),
        output,
      ),
    );

    refused(
      bindings.bluez_media_folder_search(
        handle,
        folder.cast(),
        value.cast(),
        output,
      ),
    );
    refused(
      bindings.bluez_media_folder_list_items(handle, folder.cast(), output),
    );
    refused(
      bindings.bluez_media_folder_change_folder(
        handle,
        folder.cast(),
        folder.cast(),
      ),
    );
    refused(
      bindings.bluez_media_folder_get_properties(handle, folder.cast(), output),
    );

    refused(bindings.bluez_media_item_play(handle, item.cast()));
    refused(bindings.bluez_media_item_add_to_now_playing(handle, item.cast()));
    refused(
      bindings.bluez_media_item_get_properties(handle, item.cast(), output),
    );
    refused(
      bindings.bluez_media_item_get_cover_art(
        handle,
        item.cast(),
        target.cast(),
        1,
      ),
    );
    refused(
      bindings.bluez_media_item_get_cover_art_from_existing_session(
        handle,
        item.cast(),
        target.cast(),
        1,
      ),
    );

    refused(
      bindings.bluez_media_transport_acquire(handle, transport.cast(), output),
    );
    refused(
      bindings.bluez_media_transport_try_acquire(
        handle,
        transport.cast(),
        output,
      ),
    );
    refused(bindings.bluez_media_transport_release(handle, transport.cast()));
    refused(
      bindings.bluez_media_transport_get_properties(
        handle,
        transport.cast(),
        output,
      ),
    );
    refused(
      bindings.bluez_media_transport_set_volume(handle, transport.cast(), 0),
    );
    refused(bindings.bluez_media_get_managed_objects(handle, output));
  } finally {
    bindings.bluez_media_buffer_free(output);
    calloc.free(output);
    calloc.free(registration);
    calloc.free(value);
    calloc.free(target);
    calloc.free(transport);
    calloc.free(item);
    calloc.free(folder);
    calloc.free(control);
    calloc.free(player);
    calloc.free(adapter);
  }
}
