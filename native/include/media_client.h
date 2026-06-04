// media_client.h
#pragma once

#include <map>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <string>

#include "bluez_media_native.h"

class LocalPlayer;

class MediaClient {
public:
  explicit MediaClient(sdbus::IConnection &conn);
  ~MediaClient();

  sdbus::IConnection &connection() { return conn_; }

  int register_player(const BluezMediaPlayerRegistration &registration);
  int unregister_player(const char *adapter_path, const char *player_path);

  std::vector<uint8_t> get_managed_objects() const;

private:
  sdbus::IConnection &conn_;
  std::map<std::string, std::unique_ptr<LocalPlayer>> players_;
};
