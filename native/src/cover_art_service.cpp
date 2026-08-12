#include "cover_art_service.h"

#include <filesystem>
#include <map>
#include <thread>
#include <vector>

#include "bluez_media_native.h"
#include "media_utils.h"

namespace {

constexpr auto kBluezService = "org.bluez";
constexpr auto kObexService = "org.bluez.obex";
constexpr auto kPropertiesIface = "org.freedesktop.DBus.Properties";
constexpr auto kPlayerIface = "org.bluez.MediaPlayer1";
constexpr auto kDeviceIface = "org.bluez.Device1";
constexpr auto kObexClientIface = "org.bluez.obex.Client1";
constexpr auto kObexImageIface = "org.bluez.obex.Image1";
constexpr auto kObexTransferIface = "org.bluez.obex.Transfer1";

using Properties = std::map<std::string, sdbus::Variant>;

struct CoverArtSource {
  sdbus::ObjectPath device_path;
  std::string image_handle;
  uint16_t obex_port{};
};

bool file_has_data(const std::string& path) {
  std::error_code error;
  return std::filesystem::is_regular_file(path, error) &&
         std::filesystem::file_size(path, error) > 0;
}

void remove_partial_file(const std::string& path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

Properties get_properties(sdbus::IConnection& bus,
                          const char* service,
                          const sdbus::ObjectPath& path,
                          const char* interface) {
  auto proxy = sdbus::createProxy(bus, sdbus::ServiceName{service}, path);
  Properties properties;
  proxy->callMethod("GetAll")
      .onInterface(kPropertiesIface)
      .withArguments(std::string{interface})
      .storeResultsTo(properties);
  return properties;
}

std::string get_device_address(sdbus::IConnection& system_bus,
                               const sdbus::ObjectPath& device_path) {
  return media_property<std::string>(
      get_properties(system_bus, kBluezService, device_path, kDeviceIface),
      "Address");
}

CoverArtSource get_cover_art_source(sdbus::IConnection& system_bus,
                                    const std::string& player_path) {
  const auto properties = get_properties(
      system_bus, kBluezService, sdbus::ObjectPath{player_path}, kPlayerIface);
  const auto track = media_property<Properties>(properties, "Track");
  return {
      media_property<sdbus::ObjectPath>(properties, "Device"),
      media_property<std::string>(track, "ImgHandle"),
      media_property<uint16_t>(properties, "ObexPort"),
  };
}

std::string wait_for_image_handle(sdbus::IConnection& system_bus,
                                  const std::string& player_path,
                                  std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    const auto source = get_cover_art_source(system_bus, player_path);
    if (!source.image_handle.empty()) {
      return source.image_handle;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
  }
  return {};
}

sdbus::ObjectPath create_session(sdbus::IConnection& session_bus,
                                 const std::string& device_address,
                                 uint16_t obex_port) {
  auto proxy = sdbus::createProxy(session_bus, sdbus::ServiceName{kObexService},
                                  sdbus::ObjectPath{"/org/bluez/obex"});
  Properties args{{"Target", sdbus::Variant{std::string{"bip-avrcp"}}},
                  {"PSM", sdbus::Variant{obex_port}}};
  sdbus::ObjectPath session;
  proxy->callMethod("CreateSession")
      .onInterface(kObexClientIface)
      .withArguments(device_address, args)
      .storeResultsTo(session);
  return session;
}

void remove_session(sdbus::IConnection& session_bus,
                    const sdbus::ObjectPath& session) noexcept {
  try {
    auto proxy =
        sdbus::createProxy(session_bus, sdbus::ServiceName{kObexService},
                           sdbus::ObjectPath{"/org/bluez/obex"});
    proxy->callMethod("RemoveSession")
        .onInterface(kObexClientIface)
        .withArguments(session);
  } catch (const sdbus::Error&) {
  }
}

Properties preferred_description(sdbus::IProxy& image,
                                 const std::string& image_handle) {
  std::vector<Properties> descriptions;
  image.callMethod("Properties")
      .onInterface(kObexImageIface)
      .withArguments(image_handle)
      .storeResultsTo(descriptions);
  for (const auto& description : descriptions) {
    if (media_property<std::string>(description, "type") == "variant" &&
        media_property<std::string>(description, "encoding") == "PNG") {
      return description;
    }
  }
  return {};
}

bool wait_for_transfer(sdbus::IConnection& session_bus,
                       const sdbus::ObjectPath& transfer_path,
                       const std::string& target_file,
                       std::chrono::milliseconds timeout) {
  auto transfer = sdbus::createProxy(
      session_bus, sdbus::ServiceName{kObexService}, transfer_path);
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    try {
      sdbus::Variant value;
      transfer->callMethod("Get")
          .onInterface(kPropertiesIface)
          .withArguments(std::string{kObexTransferIface}, std::string{"Status"})
          .storeResultsTo(value);
      if (value.containsValueOfType<std::string>()) {
        const auto status = value.get<std::string>();
        if (status == "complete") {
          return file_has_data(target_file);
        }
        if (status == "error") {
          return false;
        }
      }
    } catch (const sdbus::Error&) {
      return file_has_data(target_file);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
  }

  try {
    transfer->callMethod("Cancel").onInterface(kObexTransferIface);
  } catch (const sdbus::Error&) {
  }
  return false;
}

bool get_image(sdbus::IConnection& session_bus,
               sdbus::IProxy& image,
               const std::string& target_file,
               const std::string& image_handle,
               const Properties& description,
               std::chrono::milliseconds timeout) {
  sdbus::ObjectPath transfer;
  Properties transfer_properties;
  image.callMethod("Get")
      .onInterface(kObexImageIface)
      .withArguments(target_file, image_handle, description)
      .storeResultsTo(transfer, transfer_properties);
  return wait_for_transfer(session_bus, transfer, target_file, timeout);
}

bool get_thumbnail(sdbus::IConnection& session_bus,
                   sdbus::IProxy& image,
                   const std::string& target_file,
                   const std::string& image_handle,
                   std::chrono::milliseconds timeout) {
  sdbus::ObjectPath transfer;
  Properties transfer_properties;
  image.callMethod("GetThumbnail")
      .onInterface(kObexImageIface)
      .withArguments(target_file, image_handle)
      .storeResultsTo(transfer, transfer_properties);
  return wait_for_transfer(session_bus, transfer, target_file, timeout);
}

}  // namespace

CoverArtService::CoverArtService(sdbus::IConnection& system_bus)
    : system_bus_(system_bus) {}

CoverArtService::~CoverArtService() {
  const std::scoped_lock lock(mutex_);
  if (!session_bus_) {
    return;
  }
  for (const auto& [device_address, session] : sessions_) {
    remove_session(*session_bus_, session.object_path);
  }
}

sdbus::IConnection& CoverArtService::session_bus() {
  if (!session_bus_) {
    session_bus_ = sdbus::createSessionBusConnection();
  }
  return *session_bus_;
}

void CoverArtService::register_player(const std::string& player_path,
                                      const sdbus::ObjectPath& device_path,
                                      uint16_t obex_port) {
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

  const auto device_address = get_device_address(system_bus_, device_path);
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
    remove_session(session_bus(), session->second.object_path);
    session->second = {
        create_session(session_bus(), device_address, obex_port),
        obex_port,
        users,
    };
  }
  if (session == sessions_.end()) {
    session = sessions_
                  .emplace(device_address,
                           Session{create_session(session_bus(), device_address,
                                                  obex_port),
                                   obex_port, 0})
                  .first;
  }
  ++session->second.users;
  players_.emplace(player_path, Player{device_path, device_address});
}

void CoverArtService::unregister_player(
    const std::string& player_path) noexcept {
  const std::scoped_lock lock(mutex_);
  unregister_player_locked(player_path);
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
  remove_session(session_bus(), session->second.object_path);
  sessions_.erase(session);
}

int CoverArtService::get(const std::string& player_path,
                         const std::string& target_file,
                         std::chrono::milliseconds timeout) {
  if (player_path.empty() || target_file.empty() || timeout.count() <= 0 ||
      !std::filesystem::path{target_file}.is_absolute() ||
      std::filesystem::exists(target_file)) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }

  auto source = get_cover_art_source(system_bus_, player_path);
  if (source.device_path.empty() || source.obex_port == 0) {
    return BLUEZ_MEDIA_ERROR_NOT_FOUND;
  }
  register_player(player_path, source.device_path, source.obex_port);

  if (source.image_handle.empty()) {
    source.image_handle =
        wait_for_image_handle(system_bus_, player_path, timeout);
  }
  if (source.image_handle.empty()) {
    return BLUEZ_MEDIA_ERROR_NOT_FOUND;
  }

  const std::scoped_lock lock(mutex_);
  const auto player = players_.find(player_path);
  if (player == players_.end()) {
    return BLUEZ_MEDIA_ERROR_NOT_FOUND;
  }
  const auto session = sessions_.find(player->second.device_address);
  if (session == sessions_.end()) {
    return BLUEZ_MEDIA_ERROR_NOT_FOUND;
  }

  auto& bus = session_bus();
  auto image = sdbus::createProxy(bus, sdbus::ServiceName{kObexService},
                                  session->second.object_path);

  try {
    if (get_thumbnail(bus, *image, target_file, source.image_handle, timeout)) {
      return BLUEZ_MEDIA_SUCCESS;
    }
  } catch (const sdbus::Error&) {
  }
  remove_partial_file(target_file);

  Properties preferred;
  try {
    preferred = preferred_description(*image, source.image_handle);
  } catch (const sdbus::Error&) {
  }

  if (!preferred.empty()) {
    try {
      if (get_image(bus, *image, target_file, source.image_handle, preferred,
                    timeout)) {
        return BLUEZ_MEDIA_SUCCESS;
      }
    } catch (const sdbus::Error&) {
    }
    remove_partial_file(target_file);
  }

  try {
    if (get_image(bus, *image, target_file, source.image_handle, {}, timeout)) {
      return BLUEZ_MEDIA_SUCCESS;
    }
  } catch (const sdbus::Error&) {
  }
  remove_partial_file(target_file);
  return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
}
