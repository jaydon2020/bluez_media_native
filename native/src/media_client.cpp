// media_client.cpp
#include "media_client.h"
#include "bluez_media_types.h"
#include "local_player.h"

MediaClient::MediaClient(sdbus::IConnection &conn) : conn_(conn) {
  try {
    conn_.addObjectManager(sdbus::ObjectPath{"/"});
  } catch (const sdbus::Error &e) {
    fprintf(stderr, "MediaClient: failed to add ObjectManager: %s\n", e.what());
  }
}

MediaClient::~MediaClient() = default;

int MediaClient::register_player(
    const BluezMediaPlayerRegistration &registration) {
  if (registration.adapter_path == nullptr ||
      registration.player_path == nullptr) {
    return -1;
  }
  std::string player_path{registration.player_path};
  if (players_.contains(player_path)) {
    return -2;
  }

  try {
    players_.emplace(player_path,
                     std::make_unique<LocalPlayer>(conn_, registration));
    return 0;
  } catch (const std::exception &) {
    return -3;
  }
}

int MediaClient::unregister_player(const char *adapter_path,
                                   const char *player_path) {
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

std::vector<uint8_t> MediaClient::get_managed_objects() const {
  auto proxy = sdbus::createProxy(conn_, sdbus::ServiceName{"org.bluez"},
                                  sdbus::ObjectPath{"/"});
  std::map<sdbus::ObjectPath,
           std::map<std::string, std::map<std::string, sdbus::Variant>>>
      objects;
  proxy->callMethod("GetManagedObjects")
      .onInterface("org.freedesktop.DBus.ObjectManager")
      .storeResultsTo(objects);

  BlueZMediaManagedObjects result;
  for (const auto &[path, interfaces] : objects) {
    if (interfaces.contains("org.bluez.Media1")) {
      result.media.push_back(path);
    }
    if (interfaces.contains("org.bluez.MediaPlayer1")) {
      result.players.push_back(path);
    }
    if (interfaces.contains("org.bluez.MediaControl1")) {
      result.controls.push_back(path);
    }
    if (interfaces.contains("org.bluez.MediaTransport1")) {
      result.transports.push_back(path);
    }
    if (interfaces.contains("org.bluez.MediaFolder1")) {
      result.folders.push_back(path);
    }
    if (interfaces.contains("org.bluez.MediaItem1")) {
      result.items.push_back(path);
    }
  }
  return glz::encode(result);
}
