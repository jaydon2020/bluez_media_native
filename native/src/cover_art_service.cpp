#include "cover_art_service.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <vector>

#include "bluez_media_native.h"
#include "media_utils.h"

namespace {

constexpr auto kBluezService = "org.bluez";
constexpr auto kObexService = "org.bluez.obex";
constexpr auto kPropertiesIface = "org.freedesktop.DBus.Properties";
constexpr auto kPlayerIface = "org.bluez.MediaPlayer1";
constexpr auto kItemIface = "org.bluez.MediaItem1";
constexpr auto kDeviceIface = "org.bluez.Device1";
constexpr auto kObexClientIface = "org.bluez.obex.Client1";
constexpr auto kObexImageIface = "org.bluez.obex.Image1";
constexpr auto kObexSessionIface = "org.bluez.obex.Session1";
constexpr auto kObexTransferIface = "org.bluez.obex.Transfer1";
constexpr auto kObjectManagerIface = "org.freedesktop.DBus.ObjectManager";

using Properties = std::map<std::string, sdbus::Variant>;
using Clock = std::chrono::steady_clock;
using Deadline = Clock::time_point;

uint64_t remaining_timeout(Deadline deadline) {
  const auto value = std::chrono::duration_cast<std::chrono::microseconds>(
      deadline - Clock::now());
  if (value <= std::chrono::microseconds::zero()) {
    throw std::runtime_error("Cover art operation timed out");
  }
  return static_cast<uint64_t>(value.count());
}

struct CoverArtSource {
  sdbus::ObjectPath device_path;
  std::string image_handle;
  uint16_t obex_port{};
  std::string player_path;
};

bool file_has_data(const std::string& path) {
  std::error_code error;
  return std::filesystem::is_regular_file(path, error) &&
         std::filesystem::file_size(path, error) > 0;
}

bool file_has_size(const std::string& path, uint64_t expected_size) {
  std::error_code error;
  return expected_size > 0 && std::filesystem::is_regular_file(path, error) &&
         std::filesystem::file_size(path, error) == expected_size;
}

bool transfer_file_complete(const std::string& path, uint64_t expected_size) {
  return expected_size > 0 ? file_has_size(path, expected_size)
                           : file_has_data(path);
}

void remove_partial_file(const std::string& path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

struct PartialFile {
  const std::string& path;
  bool complete = false;
  ~PartialFile() {
    if (!complete)
      remove_partial_file(path);
  }
};

struct TransferCleanup {
  sdbus::IProxy& proxy;
  bool finished = false;
  ~TransferCleanup() {
    if (finished)
      return;
    try {
      proxy.callMethod("Cancel")
          .onInterface(kObexTransferIface)
          .withTimeout(uint64_t{200000});
    } catch (...) {
      // Cleanup must not replace the original timeout/transfer error.
    }
  }
};

Properties get_properties(sdbus::IConnection& bus,
                          const char* service,
                          const sdbus::ObjectPath& path,
                          const char* interface,
                          Deadline deadline) {
  auto proxy = sdbus::createProxy(bus, sdbus::ServiceName{service}, path);
  Properties properties;
  properties = proxy->callMethodAsync("GetAll")
                   .onInterface(kPropertiesIface)
                   .withArguments(std::string{interface})
                   .withTimeout(remaining_timeout(deadline))
                   .getResultAsFuture<Properties>()
                   .get();
  return properties;
}

std::string get_device_address(sdbus::IConnection& system_bus,
                               const sdbus::ObjectPath& device_path,
                               Deadline deadline) {
  return media_property<std::string>(
      get_properties(system_bus, kBluezService, device_path, kDeviceIface,
                     deadline),
      "Address");
}

CoverArtSource get_cover_art_source(sdbus::IConnection& system_bus,
                                    const std::string& object_path,
                                    Deadline deadline,
                                    bool item) {
  if (item) {
    const auto item_properties =
        get_properties(system_bus, kBluezService,
                       sdbus::ObjectPath{object_path}, kItemIface, deadline);
    const auto player_path =
        media_property<sdbus::ObjectPath>(item_properties, "Player");
    const auto metadata =
        media_property<Properties>(item_properties, "Metadata");
    const auto player_properties = get_properties(
        system_bus, kBluezService, player_path, kPlayerIface, deadline);
    return {
        media_property<sdbus::ObjectPath>(player_properties, "Device"),
        media_property<std::string>(metadata, "ImgHandle"),
        media_property<uint16_t>(player_properties, "ObexPort"),
        player_path,
    };
  }

  const auto properties =
      get_properties(system_bus, kBluezService, sdbus::ObjectPath{object_path},
                     kPlayerIface, deadline);
  const auto track = media_property<Properties>(properties, "Track");
  return {
      media_property<sdbus::ObjectPath>(properties, "Device"),
      media_property<std::string>(track, "ImgHandle"),
      media_property<uint16_t>(properties, "ObexPort"),
      object_path,
  };
}

std::string wait_for_image_handle(sdbus::IConnection& system_bus,
                                  const std::string& object_path,
                                  Deadline deadline,
                                  bool item) {
  while (Clock::now() < deadline) {
    const auto source =
        get_cover_art_source(system_bus, object_path, deadline, item);
    if (!source.image_handle.empty()) {
      return source.image_handle;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
  }
  return {};
}

sdbus::ObjectPath create_session(sdbus::IConnection& session_bus,
                                 const std::string& device_address,
                                 uint16_t obex_port,
                                 Deadline deadline) {
  auto proxy = sdbus::createProxy(session_bus, sdbus::ServiceName{kObexService},
                                  sdbus::ObjectPath{"/org/bluez/obex"});
  Properties args{{"Target", sdbus::Variant{std::string{"bip-avrcp"}}},
                  {"PSM", sdbus::Variant{obex_port}}};
  sdbus::ObjectPath session;
  proxy->callMethod("CreateSession")
      .onInterface(kObexClientIface)
      .withArguments(device_address, args)
      .withTimeout(remaining_timeout(deadline))
      .storeResultsTo(session);
  return session;
}

sdbus::ObjectPath find_session(sdbus::IConnection& session_bus,
                               const std::string& device_address,
                               uint16_t obex_port,
                               Deadline deadline) {
  using Interfaces = std::map<std::string, Properties>;
  std::map<sdbus::ObjectPath, Interfaces> objects;
  auto proxy = sdbus::createProxy(session_bus, sdbus::ServiceName{kObexService},
                                  sdbus::ObjectPath{"/"});
  proxy->callMethod("GetManagedObjects")
      .onInterface(kObjectManagerIface)
      .withTimeout(remaining_timeout(deadline))
      .storeResultsTo(objects);
  for (const auto& [path, interfaces] : objects) {
    const auto session = interfaces.find(kObexSessionIface);
    if (session != interfaces.end() &&
        media_property<std::string>(session->second, "Destination") ==
            device_address &&
        media_property<uint16_t>(session->second, "PSM") == obex_port) {
      return path;
    }
  }
  return {};
}

void remove_session(sdbus::IConnection& session_bus,
                    const sdbus::ObjectPath& session) noexcept {
  try {
    auto proxy =
        sdbus::createProxy(session_bus, sdbus::ServiceName{kObexService},
                           sdbus::ObjectPath{"/org/bluez/obex"});
    proxy->callMethod("RemoveSession")
        .onInterface(kObexClientIface)
        .withArguments(session)
        .withTimeout(uint64_t{200000});
  } catch (...) {
  }
}

Properties preferred_description(sdbus::IProxy& image,
                                 const std::string& image_handle,
                                 Deadline deadline) {
  std::vector<Properties> descriptions;
  image.callMethod("Properties")
      .onInterface(kObexImageIface)
      .withArguments(image_handle)
      .withTimeout(remaining_timeout(deadline))
      .storeResultsTo(descriptions);
  for (const auto& description : descriptions) {
    if (media_property<std::string>(description, "type") == "native") {
      return description;
    }
  }
  return {};
}

bool wait_for_transfer(sdbus::IConnection& session_bus,
                       const sdbus::ObjectPath& transfer_path,
                       const std::string& target_file,
                       uint64_t expected_size,
                       Deadline deadline) {
  auto transfer = sdbus::createProxy(
      session_bus, sdbus::ServiceName{kObexService}, transfer_path);
  TransferCleanup cleanup{*transfer};
  while (Clock::now() < deadline) {
    try {
      sdbus::Variant value;
      transfer->callMethod("Get")
          .onInterface(kPropertiesIface)
          .withArguments(std::string{kObexTransferIface}, std::string{"Status"})
          .withTimeout(remaining_timeout(deadline))
          .storeResultsTo(value);
      if (value.containsValueOfType<std::string>()) {
        const auto status = value.get<std::string>();
        if (status == "complete") {
          cleanup.finished = true;
          return transfer_file_complete(target_file, expected_size);
        }
        if (status == "error") {
          cleanup.finished = true;
          return false;
        }
      }
    } catch (const sdbus::Error& error) {
      return error.getName() == "org.freedesktop.DBus.Error.UnknownObject" &&
             transfer_file_complete(target_file, expected_size);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
  }

  return false;
}

bool get_image(sdbus::IConnection& session_bus,
               sdbus::IProxy& image,
               const std::string& target_file,
               const std::string& image_handle,
               const Properties& description,
               Deadline deadline) {
  sdbus::ObjectPath transfer;
  Properties transfer_properties;
  image.callMethod("Get")
      .onInterface(kObexImageIface)
      .withArguments(target_file, image_handle, description)
      .withTimeout(remaining_timeout(deadline))
      .storeResultsTo(transfer, transfer_properties);
  return wait_for_transfer(
      session_bus, transfer, target_file,
      media_property<uint64_t>(transfer_properties, "Size"), deadline);
}

bool get_thumbnail(sdbus::IConnection& session_bus,
                   sdbus::IProxy& image,
                   const std::string& target_file,
                   const std::string& image_handle,
                   Deadline deadline) {
  sdbus::ObjectPath transfer;
  Properties transfer_properties;
  image.callMethod("GetThumbnail")
      .onInterface(kObexImageIface)
      .withArguments(target_file, image_handle)
      .withTimeout(remaining_timeout(deadline))
      .storeResultsTo(transfer, transfer_properties);
  return wait_for_transfer(
      session_bus, transfer, target_file,
      media_property<uint64_t>(transfer_properties, "Size"), deadline);
}

}  // namespace

CoverArtService::CoverArtService(sdbus::IConnection& system_bus,
                                 std::function<void()> reconnect)
    : system_bus_(system_bus),
      session_bus_(sdbus::createSessionBusConnection()),
      reconnect_(std::move(reconnect)) {
  obex_owner_subscription_ = session_bus_->addMatch(
      "type='signal',sender='org.freedesktop.DBus',"
      "interface='org.freedesktop.DBus',member='NameOwnerChanged',"
      "arg0='org.bluez.obex'",
      [this](sdbus::Message message) {
        std::string name, previous, current;
        message >> name >> previous >> current;
        {
          const std::scoped_lock lock(mutex_);
          sessions_.clear();
        }
        if (!current.empty() && reconnect_)
          reconnect_();
      },
      sdbus::return_slot);
  session_removed_subscription_ = session_bus_->addMatch(
      "type='signal',sender='org.bluez.obex',"
      "interface='org.freedesktop.DBus.ObjectManager',"
      "member='InterfacesRemoved'",
      [this](sdbus::Message message) {
        sdbus::ObjectPath path;
        std::vector<std::string> interfaces;
        message >> path >> interfaces;
        if (std::ranges::find(interfaces, kObexSessionIface) ==
            interfaces.end()) {
          return;
        }
        bool removed = false;
        {
          const std::scoped_lock lock(mutex_);
          for (auto session = sessions_.begin(); session != sessions_.end();
               ++session) {
            if (session->second.object_path == path) {
              sessions_.erase(session);
              removed = true;
              break;
            }
          }
        }
        if (removed && reconnect_)
          reconnect_();
      },
      sdbus::return_slot);
  session_bus_->enterEventLoopAsync();
}

CoverArtService::~CoverArtService() {
  obex_owner_subscription_.reset();
  session_removed_subscription_.reset();
  reset();
  session_bus_->leaveEventLoop();
}

void CoverArtService::reset() {
  const std::scoped_lock transfer_lock(transfer_mutex_);
  const std::scoped_lock lock(mutex_);
  if (!session_bus_) {
    return;
  }
  for (const auto& [device_address, session] : sessions_) {
    if (session.owned) {
      remove_session(*session_bus_, session.object_path);
    }
  }
  sessions_.clear();
  players_.clear();
}

sdbus::IConnection& CoverArtService::session_bus() {
  return *session_bus_;
}

void CoverArtService::reconnect_players(std::chrono::milliseconds timeout) {
  std::vector<std::tuple<std::string, sdbus::ObjectPath, uint16_t>> players;
  {
    const std::scoped_lock lock(mutex_);
    players.reserve(players_.size());
    for (const auto& [path, player] : players_) {
      players.emplace_back(path, player.device_path, player.obex_port);
    }
  }
  std::exception_ptr last_error;
  for (const auto& [path, device_path, port] : players) {
    try {
      register_player(path, device_path, port, Clock::now() + timeout);
    } catch (...) {
      last_error = std::current_exception();
    }
  }
  if (last_error)
    std::rethrow_exception(last_error);
}

void CoverArtService::register_player(const std::string& player_path,
                                      const sdbus::ObjectPath& device_path,
                                      uint16_t obex_port,
                                      Deadline deadline) {
  if (player_path.empty() || device_path.empty() || obex_port == 0) {
    unregister_player(player_path);
    return;
  }

  {
    const std::scoped_lock lock(mutex_);
    const auto player = players_.find(player_path);
    if (player != players_.end() && player->second.device_path == device_path) {
      const auto session = sessions_.find(player->second.device_address);
      if (session != sessions_.end() && session->second.port == obex_port) {
        return;
      }
    }
  }

  const auto device_address =
      get_device_address(system_bus_, device_path, deadline);
  if (device_address.empty()) {
    unregister_player(player_path);
    return;
  }

  const std::scoped_lock lock(mutex_);
  const auto player = players_.find(player_path);
  if (player != players_.end() && player->second.device_path == device_path) {
    const auto session = sessions_.find(player->second.device_address);
    if (session != sessions_.end() && session->second.port == obex_port) {
      return;
    }
  }

  unregister_player_locked(player_path);
  auto session = sessions_.find(device_address);
  if (session != sessions_.end() && session->second.port != obex_port) {
    const auto users = session->second.users;
    auto replacement =
        find_session(session_bus(), device_address, obex_port, deadline);
    const bool owned = replacement.empty();
    if (owned) {
      replacement =
          create_session(session_bus(), device_address, obex_port, deadline);
    }
    if (session->second.owned) {
      remove_session(session_bus(), session->second.object_path);
    }
    session->second = {replacement, obex_port, users, owned};
  }
  if (session == sessions_.end()) {
    auto path =
        find_session(session_bus(), device_address, obex_port, deadline);
    const bool owned = path.empty();
    if (owned) {
      path = create_session(session_bus(), device_address, obex_port, deadline);
    }
    session =
        sessions_.emplace(device_address, Session{path, obex_port, 0, owned})
            .first;
  }
  ++session->second.users;
  players_.emplace(player_path, Player{device_path, device_address, obex_port});
}

void CoverArtService::unregister_player(
    const std::string& player_path) noexcept {
  const std::scoped_lock lock(mutex_);
  unregister_player_locked(player_path);
}

void CoverArtService::invalidate_player_session(
    const std::string& player_path) noexcept {
  const std::scoped_lock lock(mutex_);
  const auto player = players_.find(player_path);
  if (player == players_.end()) {
    return;
  }
  const auto address = player->second.device_address;
  for (auto current = players_.begin(); current != players_.end();) {
    if (current->second.device_address == address) {
      current = players_.erase(current);
    } else {
      ++current;
    }
  }
  const auto session = sessions_.find(address);
  if (session != sessions_.end()) {
    if (session->second.owned) {
      remove_session(session_bus(), session->second.object_path);
    }
    sessions_.erase(session);
  }
}

void CoverArtService::unregister_player_locked(
    const std::string& player_path) noexcept {
  const auto player = players_.find(player_path);
  if (player == players_.end()) {
    return;
  }
  const auto device_address = player->second.device_address;
  players_.erase(player);

  const auto session = sessions_.find(device_address);
  if (session == sessions_.end() || --session->second.users != 0) {
    return;
  }
  if (session->second.owned) {
    remove_session(session_bus(), session->second.object_path);
  }
  sessions_.erase(session);
}

int CoverArtService::get(const std::string& object_path,
                         const std::string& target_file,
                         std::chrono::milliseconds timeout) {
  return get_impl(object_path, target_file, timeout, false, ObjectKind::player);
}

int CoverArtService::get_from_existing_session(
    const std::string& object_path,
    const std::string& target_file,
    std::chrono::milliseconds timeout) {
  return get_impl(object_path, target_file, timeout, true, ObjectKind::player);
}

int CoverArtService::get_item(const std::string& object_path,
                              const std::string& target_file,
                              std::chrono::milliseconds timeout) {
  return get_impl(object_path, target_file, timeout, false, ObjectKind::item);
}

int CoverArtService::get_item_from_existing_session(
    const std::string& object_path,
    const std::string& target_file,
    std::chrono::milliseconds timeout) {
  return get_impl(object_path, target_file, timeout, true, ObjectKind::item);
}

int CoverArtService::get_impl(const std::string& object_path,
                              const std::string& target_file,
                              std::chrono::milliseconds timeout,
                              bool existing_session_only,
                              ObjectKind object_kind) {
  const std::scoped_lock transfer_lock(transfer_mutex_);
  if (object_path.empty() || target_file.empty() || timeout.count() <= 0 ||
      !std::filesystem::path{target_file}.is_absolute() ||
      std::filesystem::exists(target_file)) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }

  PartialFile partial{target_file};
  const auto deadline = Clock::now() + timeout;
  const bool item = object_kind == ObjectKind::item;
  auto source = get_cover_art_source(system_bus_, object_path, deadline, item);
  if (source.device_path.empty() || source.obex_port == 0) {
    return BLUEZ_MEDIA_ERROR_NOT_FOUND;
  }
  auto& bus = session_bus();
  const auto device_address =
      get_device_address(system_bus_, source.device_path, deadline);
  if (device_address.empty()) {
    return BLUEZ_MEDIA_ERROR_NOT_FOUND;
  }

  const int attempts = existing_session_only ? 1 : 2;
  std::exception_ptr last_dbus_error;
  for (int attempt = 0; attempt < attempts; ++attempt) {
    sdbus::ObjectPath session_path;
    if (existing_session_only) {
      session_path =
          find_session(bus, device_address, source.obex_port, deadline);
      if (session_path.empty()) {
        return BLUEZ_MEDIA_ERROR_NOT_FOUND;
      }
    } else {
      register_player(source.player_path, source.device_path, source.obex_port,
                      deadline);
      const std::scoped_lock lock(mutex_);
      const auto player = players_.find(source.player_path);
      if (player == players_.end()) {
        return BLUEZ_MEDIA_ERROR_NOT_FOUND;
      }
      const auto session = sessions_.find(player->second.device_address);
      if (session == sessions_.end()) {
        return BLUEZ_MEDIA_ERROR_NOT_FOUND;
      }
      session_path = session->second.object_path;
    }

    if (source.image_handle.empty()) {
      source.image_handle =
          wait_for_image_handle(system_bus_, object_path, deadline, item);
    }
    if (source.image_handle.empty()) {
      return BLUEZ_MEDIA_ERROR_NOT_FOUND;
    }

    try {
      auto image = sdbus::createProxy(bus, sdbus::ServiceName{kObexService},
                                      session_path);

      try {
        if (get_thumbnail(bus, *image, target_file, source.image_handle,
                          deadline)) {
          partial.complete = true;
          return BLUEZ_MEDIA_SUCCESS;
        }
      } catch (const sdbus::Error&) {
        last_dbus_error = std::current_exception();
      }
      remove_partial_file(target_file);

      const auto preferred =
          preferred_description(*image, source.image_handle, deadline);
      if (!preferred.empty() &&
          get_image(bus, *image, target_file, source.image_handle, preferred,
                    deadline)) {
        partial.complete = true;
        return BLUEZ_MEDIA_SUCCESS;
      }
    } catch (const sdbus::Error&) {
      last_dbus_error = std::current_exception();
    }
    remove_partial_file(target_file);
    if (!existing_session_only) {
      invalidate_player_session(source.player_path);
    }
  }

  if (last_dbus_error) {
    std::rethrow_exception(last_dbus_error);
  }
  return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
}
