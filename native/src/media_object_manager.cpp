#include "media_object_manager.h"

#include <utility>
#include <vector>

#include "bluez_media_types.h"
#include "media_browser_proxy.h"
#include "media_control_proxy.h"
#include "media_player_proxy.h"
#include "media_transport_proxy.h"

namespace {
constexpr auto kBluezService = "org.bluez";
constexpr auto kObjectManagerIface = "org.freedesktop.DBus.ObjectManager";
constexpr auto kPropertiesIface = "org.freedesktop.DBus.Properties";
constexpr auto kPlayerIface = "org.bluez.MediaPlayer1";
constexpr auto kControlIface = "org.bluez.MediaControl1";
constexpr auto kTransportIface = "org.bluez.MediaTransport1";
constexpr auto kFolderIface = "org.bluez.MediaFolder1";
constexpr auto kItemIface = "org.bluez.MediaItem1";

bool is_media_interface(const std::string &interface_name) {
  return interface_name == kPlayerIface || interface_name == kControlIface ||
         interface_name == kTransportIface || interface_name == kFolderIface ||
         interface_name == kItemIface;
}

void post_bytes(Dart_Port_DL port, uint8_t tag,
                const std::vector<uint8_t> &payload = {}) {
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
} // namespace

MediaObjectManager::MediaObjectManager(sdbus::IConnection &conn,
                                       Dart_Port_DL events_port)
    : conn_(conn), events_port_(events_port) {
  root_proxy_ = sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                                   sdbus::ObjectPath{"/"});
  root_proxy_->uponSignal("InterfacesAdded")
      .onInterface(kObjectManagerIface)
      .call([this](const sdbus::ObjectPath &path,
                   const InterfacesMap &interfaces) {
        on_interfaces_added(path, interfaces);
      });
  root_proxy_->uponSignal("InterfacesRemoved")
      .onInterface(kObjectManagerIface)
      .call([this](const sdbus::ObjectPath &path,
                   const std::vector<std::string> &interfaces) {
        on_interfaces_removed(path, interfaces);
      });
}

MediaObjectManager::~MediaObjectManager() = default;

void MediaObjectManager::get_managed_objects() {
  std::map<sdbus::ObjectPath, InterfacesMap> objects;
  root_proxy_->callMethod("GetManagedObjects")
      .onInterface(kObjectManagerIface)
      .storeResultsTo(objects);

  for (const auto &[path, interfaces] : objects) {
    on_interfaces_added(path, interfaces);
  }
  post_bytes(events_port_, 0x00);
}

void MediaObjectManager::on_interfaces_added(const sdbus::ObjectPath &path,
                                             const InterfacesMap &interfaces) {
  auto &known_interfaces = interfaces_by_path_[path];
  for (const auto &[interface_name, properties] : interfaces) {
    (void)properties;
    if (!is_media_interface(interface_name)) {
      continue;
    }
    known_interfaces.insert(interface_name);
    post_properties(path, interface_name);
  }
  if (!known_interfaces.empty()) {
    subscribe_properties(path);
  }
}

void MediaObjectManager::on_interfaces_removed(
    const sdbus::ObjectPath &path, const std::vector<std::string> &interfaces) {
  auto known = interfaces_by_path_.find(path);
  for (const auto &interface_name : interfaces) {
    if (!is_media_interface(interface_name)) {
      continue;
    }
    post_removed(path, interface_name);
    if (known != interfaces_by_path_.end()) {
      known->second.erase(interface_name);
    }
  }
  if (known != interfaces_by_path_.end() && known->second.empty()) {
    interfaces_by_path_.erase(known);
    property_proxies_.erase(path);
  }
}

void MediaObjectManager::subscribe_properties(const std::string &path) {
  if (property_proxies_.contains(path)) {
    return;
  }
  auto proxy = sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                                  sdbus::ObjectPath{path});
  proxy->uponSignal("PropertiesChanged")
      .onInterface(kPropertiesIface)
      .call([this, path](const std::string &interface_name,
                         const std::map<std::string, sdbus::Variant> &changed,
                         const std::vector<std::string> &invalidated) {
        (void)changed;
        (void)invalidated;
        if (is_media_interface(interface_name)) {
          post_properties(path, interface_name);
        }
      });
  property_proxies_.emplace(path, std::move(proxy));
}

void MediaObjectManager::post_properties(const std::string &path,
                                         const std::string &interface_name) {
  try {
    if (interface_name == kPlayerIface) {
      post_bytes(events_port_, 0x01,
                 MediaPlayerProxy{conn_, path}.properties());
    } else if (interface_name == kControlIface) {
      post_bytes(events_port_, 0x02,
                 MediaControlProxy{conn_, path}.properties());
    } else if (interface_name == kTransportIface) {
      post_bytes(events_port_, 0x04,
                 MediaTransportProxy{conn_, path}.properties());
    } else if (interface_name == kFolderIface) {
      post_bytes(events_port_, 0x05,
                 MediaBrowserProxy{conn_}.folder_properties(path));
    } else if (interface_name == kItemIface) {
      post_bytes(events_port_, 0x06,
                 MediaBrowserProxy{conn_}.item_properties(path));
    }
  } catch (const sdbus::Error &) {
  }
}

void MediaObjectManager::post_removed(const std::string &path,
                                      const std::string &interface_name) {
  BlueZMediaObjectRemoved removed;
  removed.objectPath = path;
  removed.interfaceName = interface_name;
  post_bytes(events_port_, 0x7E, glz::encode(removed));
}
