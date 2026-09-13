#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "bluez_media_native.h"
#include "dart_api_dl.h"

namespace {
std::mutex completion_mutex;
std::condition_variable completion_ready;
bool closed = false;

bool post_result(Dart_Port_DL port, Dart_CObject* message) {
  if (port != 1) return false;  // Simulate a closed Dart result port.
  assert(message->type == Dart_CObject_kTypedData);
  assert(message->value.as_typed_data.values[0] == 0xFF);
  {
    const std::scoped_lock lock(completion_mutex);
    closed = true;
  }
  completion_ready.notify_one();
  return true;
}
}  // namespace

int main() {
  const auto* private_bus = std::getenv("BLUEZ_TEST_BUS");
  if (!private_bus || std::string{private_bus} != "1") return 77;
  Dart_PostCObject_DL = post_result;
  void* client = bluez_media_client_create(0);
  assert(client);
  std::vector<std::thread> callers;
  for (int worker = 0; worker < 3; ++worker) {
    callers.emplace_back([client, worker] {
      for (int i = 0; i < 10; ++i) {
        const auto path = "/native_" + std::to_string(worker) + "_" + std::to_string(i);
        const BluezMediaPlayerRegistration registration{
            "/", path.c_str(), "Test", "Audio", "", 0, 0};
        assert(bluez_media_register_player(client, &registration) == 0);
        BluezMediaBuffer properties{};
        assert(bluez_media_player_get_properties(client, "/player", &properties) == 0);
        assert(properties.data && properties.length > 0);
        bluez_media_buffer_free(&properties);
        assert(!properties.data && properties.length == 0);
        assert(bluez_media_unregister_player(client, "/", path.c_str()) == 0);
      }
    });
  }
  for (auto& caller : callers) caller.join();
  // A dropped acquisition must release its transferred descriptor. Closing
  // must drain this call before reporting completion.
  bluez_media_call_async(client, BLUEZ_MEDIA_OP_TRANSPORT_ACQUIRE,
                        "/transport", "", 0, 2);
  bluez_media_client_destroy_async(client, 1);
  {
    std::unique_lock lock(completion_mutex);
    assert(completion_ready.wait_for(lock, std::chrono::seconds(10), [] { return closed; }));
  }
  assert(bluez_media_player_play(client, "/player") == BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT);
  // Exercise static teardown with both an abandoned live client and creation
  // whose Dart result port has closed before delivery.
  assert(bluez_media_client_create(0));
  bluez_media_client_create_async(0, 2);
}
