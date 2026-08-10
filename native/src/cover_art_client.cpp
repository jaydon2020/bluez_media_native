#include "cover_art_client.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <thread>
#include <vector>

#include "bluez_media_native.h"
#include "media_utils.h"

namespace bluez_media {
namespace {

constexpr auto kBluezService = "org.bluez";
constexpr auto kObexService = "org.bluez.obex";
constexpr auto kPropertiesIface = "org.freedesktop.DBus.Properties";
constexpr auto kObjectManagerIface = "org.freedesktop.DBus.ObjectManager";
constexpr auto kPlayerIface = "org.bluez.MediaPlayer1";
constexpr auto kDeviceIface = "org.bluez.Device1";
constexpr auto kObexClientIface = "org.bluez.obex.Client1";
constexpr auto kObexSessionIface = "org.bluez.obex.Session1";
constexpr auto kObexImageIface = "org.bluez.obex.Image1";
constexpr auto kObexTransferIface = "org.bluez.obex.Transfer1";

using Properties = std::map<std::string, sdbus::Variant>;
using Interfaces = std::map<std::string, Properties>;
using ManagedObjects = std::map<sdbus::ObjectPath, Interfaces>;

std::string lowercase(std::string value) {
  std::ranges::transform(value, value.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

bool file_has_data(const std::string& path) {
  std::error_code error;
  return std::filesystem::is_regular_file(path, error) &&
         std::filesystem::file_size(path, error) > 0;
}

void remove_partial_file(const std::string& path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

std::string get_device_address(sdbus::IConnection& system_bus,
                               const sdbus::ObjectPath& device_path) {
  auto proxy = sdbus::createProxy(system_bus, sdbus::ServiceName{kBluezService},
                                  device_path);
  Properties properties;
  proxy->callMethod("GetAll")
      .onInterface(kPropertiesIface)
      .withArguments(std::string{kDeviceIface})
      .storeResultsTo(properties);
  return media_property<std::string>(properties, "Address");
}

std::pair<std::string, std::string> get_cover_art_source(
    sdbus::IConnection& system_bus,
    const std::string& player_path) {
  auto proxy = sdbus::createProxy(system_bus, sdbus::ServiceName{kBluezService},
                                  sdbus::ObjectPath{player_path});
  Properties properties;
  proxy->callMethod("GetAll")
      .onInterface(kPropertiesIface)
      .withArguments(std::string{kPlayerIface})
      .storeResultsTo(properties);

  const auto track = media_property<Properties>(properties, "Track");
  const auto image_handle = media_property<std::string>(track, "ImgHandle");
  const auto device = media_property<sdbus::ObjectPath>(properties, "Device");
  if (image_handle.empty() || device.empty()) {
    return {};
  }
  return {get_device_address(system_bus, device), image_handle};
}

sdbus::ObjectPath find_session(sdbus::IConnection& session_bus,
                               const std::string& device_address) {
  auto proxy = sdbus::createProxy(session_bus, sdbus::ServiceName{kObexService},
                                  sdbus::ObjectPath{"/"});
  ManagedObjects objects;
  proxy->callMethod("GetManagedObjects")
      .onInterface(kObjectManagerIface)
      .storeResultsTo(objects);

  const auto destination = lowercase(device_address);
  for (const auto& [path, interfaces] : objects) {
    const auto session = interfaces.find(kObexSessionIface);
    if (session == interfaces.end()) {
      continue;
    }
    const auto candidate =
        lowercase(media_property<std::string>(session->second, "Destination"));
    const auto target =
        lowercase(media_property<std::string>(session->second, "Target"));
    if (candidate == destination &&
        (target.contains("111a") || target.contains("bip-avrcp"))) {
      return path;
    }
  }
  return {};
}

sdbus::ObjectPath create_session(sdbus::IConnection& session_bus,
                                 const std::string& device_address) {
  auto proxy = sdbus::createProxy(session_bus, sdbus::ServiceName{kObexService},
                                  sdbus::ObjectPath{"/org/bluez/obex"});
  Properties args{{"Target", sdbus::Variant{std::string{"bip-avrcp"}}}};
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

int get_cover_art(sdbus::IConnection& system_bus,
                  const std::string& player_path,
                  const std::string& target_file,
                  std::chrono::milliseconds timeout) {
  if (player_path.empty() || target_file.empty() || timeout.count() <= 0 ||
      !std::filesystem::path{target_file}.is_absolute() ||
      std::filesystem::exists(target_file)) {
    return BLUEZ_MEDIA_ERROR_INVALID_ARGUMENT;
  }

  const auto [device_address, image_handle] =
      get_cover_art_source(system_bus, player_path);
  if (device_address.empty() || image_handle.empty()) {
    return BLUEZ_MEDIA_ERROR_NOT_FOUND;
  }

  auto session_bus = sdbus::createSessionBusConnection();
  sdbus::ObjectPath session;
  try {
    session = find_session(*session_bus, device_address);
  } catch (const sdbus::Error&) {
  }
  const bool owns_session = session.empty();
  if (owns_session) {
    session = create_session(*session_bus, device_address);
  }

  try {
    auto image = sdbus::createProxy(*session_bus,
                                    sdbus::ServiceName{kObexService}, session);
    Properties preferred;
    try {
      preferred = preferred_description(*image, image_handle);
    } catch (const sdbus::Error&) {
    }

    if (!preferred.empty()) {
      try {
        if (get_image(*session_bus, *image, target_file, image_handle,
                      preferred, timeout)) {
          if (owns_session)
            remove_session(*session_bus, session);
          return BLUEZ_MEDIA_SUCCESS;
        }
      } catch (const sdbus::Error&) {
      }
      remove_partial_file(target_file);
    }

    try {
      if (get_image(*session_bus, *image, target_file, image_handle, {},
                    timeout)) {
        if (owns_session)
          remove_session(*session_bus, session);
        return BLUEZ_MEDIA_SUCCESS;
      }
    } catch (const sdbus::Error&) {
    }
    remove_partial_file(target_file);

    try {
      if (get_thumbnail(*session_bus, *image, target_file, image_handle,
                        timeout)) {
        if (owns_session)
          remove_session(*session_bus, session);
        return BLUEZ_MEDIA_SUCCESS;
      }
    } catch (const sdbus::Error&) {
    }
    remove_partial_file(target_file);
  } catch (...) {
    if (owns_session)
      remove_session(*session_bus, session);
    throw;
  }

  if (owns_session)
    remove_session(*session_bus, session);
  return BLUEZ_MEDIA_ERROR_OPERATION_FAILED;
}

}  // namespace bluez_media
