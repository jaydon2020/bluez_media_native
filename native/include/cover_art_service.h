#pragma once

#include <sdbus-c++/sdbus-c++.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

class CoverArtService {
 public:
  explicit CoverArtService(sdbus::IConnection& system_bus);
  ~CoverArtService();

  void register_player(const std::string& player_path,
                       const sdbus::ObjectPath& device_path,
                       uint16_t obex_port,
                       std::chrono::steady_clock::time_point deadline);
  void unregister_player(const std::string& player_path) noexcept;

  int get(const std::string& player_path,
          const std::string& target_file,
          std::chrono::milliseconds timeout);

 private:
  struct Player {
    sdbus::ObjectPath device_path;
    std::string device_address;
  };

  struct Session {
    sdbus::ObjectPath object_path;
    uint16_t port{};
    std::size_t users{};
  };

  sdbus::IConnection& session_bus();
  void unregister_player_locked(const std::string& player_path) noexcept;

  sdbus::IConnection& system_bus_;
  std::unique_ptr<sdbus::IConnection> session_bus_;
  std::map<std::string, Player> players_;
  std::map<std::string, Session> sessions_;
  std::mutex mutex_;
};
