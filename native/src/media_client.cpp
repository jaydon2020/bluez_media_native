// media_client.cpp
#include "media_client.h"
#include "local_player.h"

MediaClient::MediaClient(sdbus::IConnection &conn) : conn_(conn) {
  try {
    conn_.addObjectManager(sdbus::ObjectPath{"/"});
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "MediaClient: failed to add ObjectManager: %s\n", e.what());
  }
}

MediaClient::~MediaClient() = default;

int MediaClient::register_player(const BluezMediaPlayerRegistration &registration) {
  if (registration.adapter_path == nullptr || registration.player_path == nullptr) {
    return -1;
  }
  std::string player_path{registration.player_path};
  if (players_.contains(player_path)) {
    return -2;
  }

  try {
    players_.emplace(player_path, std::make_unique<LocalPlayer>(conn_, registration));
    return 0;
  } catch (const std::exception &) {
    return -3;
  }
}

int MediaClient::unregister_player(const char *adapter_path, const char *player_path) {
  if (adapter_path == nullptr || player_path == nullptr) {
    return -1;
  }
  std::string player{player_path};
  auto it = players_.find(player);
  if (it == players_.end()) {
    return -2;
  }
  players_.erase(it);
  return 0;
}
