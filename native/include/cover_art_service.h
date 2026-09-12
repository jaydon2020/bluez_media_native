#pragma once

#include <sdbus-c++/sdbus-c++.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class CoverArtService {
 public:
  explicit CoverArtService(sdbus::IConnection& system_bus,
                           std::function<void()> reconnect = {});
  ~CoverArtService();
  void reset();
  void reconnect_players(std::chrono::milliseconds timeout);

  void register_player(const std::string& player_path,
                       const sdbus::ObjectPath& device_path,
                       uint16_t obex_port,
                       std::chrono::steady_clock::time_point deadline);
  void unregister_player(const std::string& player_path) noexcept;

  int get(const std::string& object_path,
          const std::string& target_file,
          std::chrono::milliseconds timeout);
  int get_from_existing_session(const std::string& object_path,
                                const std::string& target_file,
                                std::chrono::milliseconds timeout);
  int get_item(const std::string& object_path,
               const std::string& target_file,
               std::chrono::milliseconds timeout);
  int get_item_from_existing_session(const std::string& object_path,
                                     const std::string& target_file,
                                     std::chrono::milliseconds timeout);
  bool mpris_proxy_running(std::chrono::milliseconds timeout);
  std::vector<uint8_t> get_mpris_cover_art(const std::string& item_path,
                                           std::chrono::milliseconds timeout);

 private:
  enum class ObjectKind { player, item };

  struct Player {
    sdbus::ObjectPath device_path;
    std::string device_address;
    uint16_t obex_port{};
  };

  struct Session {
    sdbus::ObjectPath object_path;
    uint16_t port{};
    std::size_t users{};
    bool owned{};
  };

  sdbus::IConnection& session_bus();
  int get_impl(const std::string& object_path,
               const std::string& target_file,
               std::chrono::milliseconds timeout,
               bool existing_session_only,
               ObjectKind object_kind);
  void invalidate_player_session(const std::string& player_path) noexcept;
  void unregister_player_locked(const std::string& player_path) noexcept;

  sdbus::IConnection& system_bus_;
  std::unique_ptr<sdbus::IConnection> session_bus_;
  sdbus::Slot obex_owner_subscription_;
  sdbus::Slot session_removed_subscription_;
  std::function<void()> reconnect_;
  std::map<std::string, Player> players_;
  std::map<std::string, Session> sessions_;
  std::mutex mutex_;
  std::mutex transfer_mutex_;
};
