#include "media_object_manager.h"

#include <utility>
#include <vector>

#include "bluez_media_types.h"
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
                const std::vector<uint8_t>& payload = {}) noexcept {
  try {
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
  } catch (...) {
  }
}
}  // namespace

MediaObjectManager::MediaObjectManager(sdbus::IConnection& conn,
                                       Dart_Port_DL events_port,
                                       RemovedCallback removed,
                                       PlayerCallback player)
    : conn_(conn),
      events_port_(events_port),
      removed_(std::move(removed)),
      player_(std::move(player)) {
  // Install a service-wide match before requesting the snapshot. Per-object
  // matches installed after discovery lose changes while GetManagedObjects
  // runs.
  properties_subscription_ = conn_.addMatch(
      "type='signal',sender='org.bluez',"
      "interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'",
      [this](sdbus::Message message) {
        std::string interface_name;
        std::map<std::string, sdbus::Variant> changed;
        std::vector<std::string> invalidated;
        message >> interface_name >> changed >> invalidated;
        const std::string path = message.getPath();
        apply_update([this, path, interface_name, changed, invalidated] {
          const auto known = interfaces_by_path_.find(path);
          if (known == interfaces_by_path_.end() ||
              !known->second.contains(interface_name))
            return;
          auto& properties = properties_by_path_.at(path).at(interface_name);
          for (const auto& [name, value] : changed) {
            properties.insert_or_assign(name, value);
          }
          const auto revision = ++next_revision_;
          revisions_[path][interface_name] = revision;
          if (!changed.empty())
            post_properties(path, interface_name);
          if (invalidated.empty())
            return;
          // Retain the last known value until GetAll supplies an authoritative
          // replacement. Invalidated does not mean zero, false or absent.
          refresh_properties(path, interface_name, revision);
        });
      },
      sdbus::return_slot);
  root_proxy_ = sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                                   sdbus::ObjectPath{"/"});
  root_proxy_->uponSignal("InterfacesAdded")
      .onInterface(kObjectManagerIface)
      .call([this](const sdbus::ObjectPath& path,
                   const InterfacesMap& interfaces) {
        apply_update([this, path, interfaces] {
          on_interfaces_added(path, interfaces);
        });
      });
  root_proxy_->uponSignal("InterfacesRemoved")
      .onInterface(kObjectManagerIface)
      .call([this](const sdbus::ObjectPath& path,
                   const std::vector<std::string>& interfaces) {
        apply_update([this, path, interfaces] {
          on_interfaces_removed(path, interfaces);
        });
      });
  owner_subscription_ = conn_.addMatch(
      "type='signal',sender='org.freedesktop.DBus',"
      "interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='org."
      "bluez'",
      [this](sdbus::Message message) {
        std::string name, previous, current;
        message >> name >> previous >> current;
        const auto generation = ++owner_generation_;
        pending_updates_.clear();
        resynchronizing_ = !current.empty();
        while (!interfaces_by_path_.empty()) {
          const auto path = interfaces_by_path_.begin()->first;
          const auto interfaces = interfaces_by_path_.begin()->second;
          on_interfaces_removed(sdbus::ObjectPath{path},
                                {interfaces.begin(), interfaces.end()});
        }
        if (removed_)
          removed_("", "org.bluez.Media1");
        post_bytes(events_port_, 0x30);
        if (current.empty())
          return;
        root_proxy_->callMethodAsync("GetManagedObjects")
            .onInterface(kObjectManagerIface)
            .uponReplyInvoke(
                [this, generation](
                    const std::optional<sdbus::Error>& error,
                    const std::map<sdbus::ObjectPath, InterfacesMap>& objects) {
                  if (generation != owner_generation_)
                    return;
                  resynchronizing_ = false;
                  if (error) {
                    pending_updates_.clear();
                    return;
                  }
                  for (const auto& [path, interfaces] : objects)
                    on_interfaces_added(path, interfaces);
                  auto updates = std::move(pending_updates_);
                  pending_updates_.clear();
                  for (auto& update : updates)
                    update();
                  post_bytes(events_port_, 0x31);
                });
      },
      sdbus::return_slot);
}

MediaObjectManager::~MediaObjectManager() = default;

void MediaObjectManager::refresh_properties(const std::string& path,
                                            const std::string& interface_name,
                                            uint64_t revision) {
  auto& proxy = property_proxies_[path];
  if (!proxy)
    proxy = sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                               sdbus::ObjectPath{path});
  proxy->callMethodAsync("GetAll")
      .onInterface(kPropertiesIface)
      .withArguments(interface_name)
      .uponReplyInvoke([this, path, interface_name, revision](
                           const std::optional<sdbus::Error>& error,
                           const std::map<std::string, sdbus::Variant>& fresh) {
        const auto current = revisions_.find(path);
        if (error || current == revisions_.end())
          return;
        const auto version = current->second.find(interface_name);
        if (version == current->second.end())
          return;
        if (version->second != revision) {
          // A concurrent change made this reply stale, but the original
          // invalidation still needs a replacement.
          refresh_properties(path, interface_name, version->second);
          return;
        }
        properties_by_path_[path][interface_name] = fresh;
        post_properties(path, interface_name);
      });
}

void MediaObjectManager::apply_update(std::function<void()> update) {
  if (resynchronizing_)
    pending_updates_.push_back(std::move(update));
  else
    update();
}

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
  for (const auto& [interface_name, properties] : interfaces) {
    if (!is_media_interface(interface_name)) {
      continue;
    }
    interfaces_by_path_[path].insert(interface_name);
    revisions_[path][interface_name] = ++next_revision_;
    properties_by_path_[path][interface_name] = properties;
    post_properties(path, interface_name);
  }
}

void MediaObjectManager::on_interfaces_removed(
    const sdbus::ObjectPath& path,
    const std::vector<std::string>& interfaces) {
  if (removed_) {
    for (const auto& interface_name : interfaces)
      removed_(path, interface_name);
  }
  auto known = interfaces_by_path_.find(path);
  if (known == interfaces_by_path_.end()) {
    return;
  }
  auto properties = properties_by_path_.find(path);
  for (const auto& interface_name : interfaces) {
    if (!is_media_interface(interface_name) ||
        known->second.erase(interface_name) == 0) {
      continue;
    }
    revisions_[path].erase(interface_name);
    post_removed(path, interface_name);
    if (properties != properties_by_path_.end()) {
      properties->second.erase(interface_name);
    }
  }
  if (known->second.empty()) {
    interfaces_by_path_.erase(known);
    revisions_.erase(path);
    property_proxies_.erase(path);
    properties_by_path_.erase(path);
  }
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
    if (player_) {
      player_(path, media_property<sdbus::ObjectPath>(properties, "Device"),
              media_property<uint16_t>(properties, "ObexPort"));
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
