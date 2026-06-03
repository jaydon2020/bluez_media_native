// media_client.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <map>
#include <memory>
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

private:
  sdbus::IConnection &conn_;
  std::map<std::string, std::unique_ptr<LocalPlayer>> players_;
};
