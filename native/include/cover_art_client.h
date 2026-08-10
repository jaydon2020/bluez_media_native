#pragma once

#include <sdbus-c++/sdbus-c++.h>

#include <chrono>
#include <string>

namespace bluez_media {

int get_cover_art(sdbus::IConnection& system_bus,
                  const std::string& player_path,
                  const std::string& target_file,
                  std::chrono::milliseconds timeout);

}  // namespace bluez_media
