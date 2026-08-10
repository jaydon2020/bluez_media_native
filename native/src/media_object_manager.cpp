#include "media_object_manager.h"

#include <utility>
#include <vector>

#include "bluez_media_types.h"
#include "cover_art_service.h"
#include "media_browser_proxy.h"
#include "media_control_proxy.h"
#include "media_player_proxy.h"
#include "media_transport_proxy.h"
#include "media_utils.h"

namespace {
constexpr auto kBluezService = "org.bluez";
constexpr auto kObjectManagerIface = "org.freedesktop.DBus.ObjectManager";
constexpr auto kPropertiesIface = "org.freedesktop.DBus.Properties";
constexpr auto kPlayerIface = "org.bluez.MediaPlayer1";
constexpr auto kControlIface = "org.bluez.MediaControl1";
constexpr auto kTransportIface = "org.bluez.MediaTransport1";
constexpr auto kFolderIface = "org.bluez.MediaFolder1";
constexpr auto kItemIface = "org.bluez.MediaItem1";

bool is_media_interface(const std::string& interface_name) {
  return interface_name == kPlayerIface || interface_name == kControlIface ||
         interface_name == kTransportIface || interface_name == kFolderIface ||
         interface_name == kItemIface;
}

void post_bytes(Dart_Port_DL port,
                uint8_t tag,
                const std::vector<uint8_t>& payload = {}) {
  std::vector<uint8_t> message;
  message.reserve(payload.size() + 1);
  message.push_back(tag);
  message.insert(message.end(), payload.begin(), payload.end());

  Dart_CObject object;
  object.type = Dart_CObject_kTypedData;
  object.value.as_typed_data.type = Dart_TypedData_kUint8;
  object.value.as_typed_data.length = static_cast<intptr_t>(message.size());
  object.value.as_typed_data.values = message.data();
  Dart_PostCObject_DL(port, &object);
}
}  // namespace

MediaObjectManager::MediaObjectManager(sdbus::IConnection& conn,
                                       Dart_Port_DL events_port,
                                       CoverArtService& cover_art)
    : conn_(conn), events_port_(events_port), cover_art_(cover_art) {
  root_proxy_ = sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                                   sdbus::ObjectPath{"/"});
  root_proxy_->uponSignal("InterfacesAdded")
      .onInterface(kObjectManagerIface)
      .call([this](const sdbus::ObjectPath& path,
                   const InterfacesMap& interfaces) {
        on_interfaces_added(path, interfaces);
      });
  root_proxy_->uponSignal("InterfacesRemoved")
      .onInterface(kObjectManagerIface)
      .call([this](const sdbus::ObjectPath& path,
                   const std::vector<std::string>& interfaces) {
        on_interfaces_removed(path, interfaces);
      });
}

MediaObjectManager::~MediaObjectManager() = default;

void MediaObjectManager::get_managed_objects() {
  std::map<sdbus::ObjectPath, InterfacesMap> objects;
  root_proxy_->callMethod("GetManagedObjects")
      .onInterface(kObjectManagerIface)
      .storeResultsTo(objects);

  for (const auto& [path, interfaces] : objects) {
    on_interfaces_added(path, interfaces);
  }
  post_bytes(events_port_, 0x00);
}

void MediaObjectManager::on_interfaces_added(const sdbus::ObjectPath& path,
                                             const InterfacesMap& interfaces) {
  auto& known_interfaces = interfaces_by_path_[path];
  for (const auto& [interface_name, properties] : interfaces) {
    if (!is_media_interface(interface_name)) {
      continue;
    }
    known_interfaces.insert(interface_name);
    properties_by_path_[path][interface_name] = properties;
    post_properties(path, interface_name);
  }
  if (!known_interfaces.empty()) {
    subscribe_properties(path);
  }
}

void MediaObjectManager::on_interfaces_removed(
    const sdbus::ObjectPath& path,
    const std::vector<std::string>& interfaces) {
  auto known = interfaces_by_path_.find(path);
  for (const auto& interface_name : interfaces) {
    if (!is_media_interface(interface_name)) {
      continue;
    }
    if (interface_name == kPlayerIface) {
      cover_art_.unregister_player(path);
    }
    post_removed(path, interface_name);
    if (known != interfaces_by_path_.end()) {
      known->second.erase(interface_name);
    }
    properties_by_path_[path].erase(interface_name);
  }
  if (known != interfaces_by_path_.end() && known->second.empty()) {
    interfaces_by_path_.erase(known);
    property_proxies_.erase(path);
    properties_by_path_.erase(path);
  }
}

void MediaObjectManager::subscribe_properties(const std::string& path) {
  if (property_proxies_.contains(path)) {
    return;
  }
  auto proxy = sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                                  sdbus::ObjectPath{path});
  proxy->uponSignal("PropertiesChanged")
      .onInterface(kPropertiesIface)
      .call([this, path](const std::string& interface_name,
                         const std::map<std::string, sdbus::Variant>& changed,
                         const std::vector<std::string>& invalidated) {
        if (!is_media_interface(interface_name)) {
          return;
        }
        auto& properties = properties_by_path_[path][interface_name];
        for (const auto& [name, value] : changed) {
          properties.insert_or_assign(name, value);
        }
        for (const auto& name : invalidated) {
          properties.erase(name);
        }
        post_properties(path, interface_name);
      });
  property_proxies_.emplace(path, std::move(proxy));
}

void MediaObjectManager::post_properties(const std::string& path,
                                         const std::string& interface_name) {
  const auto path_it = properties_by_path_.find(path);
  if (path_it == properties_by_path_.end()) {
    return;
  }
  const auto interface_it = path_it->second.find(interface_name);
  if (interface_it == path_it->second.end()) {
    return;
  }
  const auto& properties = interface_it->second;
  if (interface_name == kPlayerIface) {
    try {
      cover_art_.register_player(
          path, media_property<sdbus::ObjectPath>(properties, "Device"),
          media_property<uint16_t>(properties, "ObexPort"));
    } catch (const sdbus::Error&) {
      // Cover art is optional and must not break media discovery.
    }
    post_bytes(events_port_, 0x01,
               MediaPlayerProxy::encode_properties(path, properties));
  } else if (interface_name == kControlIface) {
    post_bytes(events_port_, 0x02,
               MediaControlProxy::encode_properties(path, properties));
  } else if (interface_name == kTransportIface) {
    post_bytes(events_port_, 0x04,
               MediaTransportProxy::encode_properties(path, properties));
  } else if (interface_name == kFolderIface) {
    post_bytes(events_port_, 0x05,
               MediaBrowserProxy::encode_folder_properties(path, properties));
  } else if (interface_name == kItemIface) {
    post_bytes(events_port_, 0x06,
               MediaBrowserProxy::encode_item_properties(path, properties));
  }
}

void MediaObjectManager::post_removed(const std::string& path,
                                      const std::string& interface_name) {
  BlueZMediaObjectRemoved removed;
  removed.objectPath = path;
  removed.interfaceName = interface_name;
  post_bytes(events_port_, 0x7E, glz::encode(removed));
}
