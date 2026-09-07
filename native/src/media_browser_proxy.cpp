// media_browser_proxy.cpp
#include "media_browser_proxy.h"
#include "bluez_media_native.h"
#include "media_utils.h"

MediaBrowserProxy::MediaBrowserProxy(sdbus::IConnection& conn) : conn_(conn) {}

std::unique_ptr<sdbus::IProxy> MediaBrowserProxy::make_folder_proxy(
    const std::string& folder_path) const {
  if (folder_path.empty()) {
    throw sdbus::Error{sdbus::Error::Name{"org.bluez.Error.InvalidArguments"},
                       "folder_path is required"};
  }
  return sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                            sdbus::ObjectPath{folder_path});
}

std::unique_ptr<sdbus::IProxy> MediaBrowserProxy::make_item_proxy(
    const std::string& item_path) const {
  if (item_path.empty()) {
    throw sdbus::Error{sdbus::Error::Name{"org.bluez.Error.InvalidArguments"},
                       "item_path is required"};
  }
  return sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                            sdbus::ObjectPath{item_path});
}

BlueZMediaItemProps MediaBrowserProxy::item_from_properties(
    const std::string& object_path,
    const std::map<std::string, sdbus::Variant>& props) {
  BlueZMediaItemProps item;
  item.objectPath = object_path;
  if (const auto it = props.find("Player"); it != props.end()) {
    item.player = variant_to_string(it->second);
  }
  if (const auto it = props.find("Name"); it != props.end()) {
    item.name = variant_to_string(it->second);
  }
  if (const auto it = props.find("Type"); it != props.end()) {
    item.type = variant_to_string(it->second);
  }
  if (const auto it = props.find("FolderType"); it != props.end()) {
    item.folderType = variant_to_string(it->second);
  }
  if (const auto it = props.find("Playable");
      it != props.end() && it->second.containsValueOfType<bool>()) {
    item.playable = it->second.get<bool>();
  }
  if (const auto it = props.find("Metadata");
      it != props.end() &&
      it->second.containsValueOfType<std::map<std::string, sdbus::Variant>>()) {
    item.metadata = track_to_properties(
        it->second.get<std::map<std::string, sdbus::Variant>>());
  }
  return item;
}

std::vector<uint8_t> MediaBrowserProxy::folder_search(
    const std::string& folder_path,
    const std::string& value) const {
  auto proxy = make_folder_proxy(folder_path);
  sdbus::ObjectPath result;
  media_call(*proxy, kMediaFolderIface, "Search", value, std::map<std::string, sdbus::Variant>{}) >> result;

  BlueZMediaFolderProps props;
  props.objectPath = result;
  return glz::encode(props);
}

std::vector<uint8_t> MediaBrowserProxy::folder_list_items(
    const std::string& folder_path) const {
  auto proxy = make_folder_proxy(folder_path);
  std::map<sdbus::ObjectPath, std::map<std::string, sdbus::Variant>> result;
  media_call(*proxy, kMediaFolderIface, "ListItems", std::map<std::string, sdbus::Variant>{}) >> result;

  BlueZMediaFolderItems items;
  items.objectPath = folder_path;
  items.items.reserve(result.size());
  for (const auto& [path, props] : result) {
    items.items.push_back(item_from_properties(path, props));
  }
  return glz::encode(items);
}

int MediaBrowserProxy::folder_change_folder(
    const std::string& folder_path,
    const std::string& target_folder_path) const {
  media_call(*make_folder_proxy(folder_path), kMediaFolderIface, "ChangeFolder", sdbus::ObjectPath{target_folder_path});
  return BLUEZ_MEDIA_SUCCESS;
}

std::vector<uint8_t> MediaBrowserProxy::folder_properties(
    const std::string& folder_path) const {
  auto proxy = make_folder_proxy(folder_path);
  std::map<std::string, sdbus::Variant> properties;
  media_call(*proxy, "org.freedesktop.DBus.Properties", "GetAll", std::string{kMediaFolderIface}) >> properties;
  return encode_folder_properties(folder_path, properties);
}

std::vector<uint8_t> MediaBrowserProxy::encode_folder_properties(
    const std::string& folder_path,
    const std::map<std::string, sdbus::Variant>& properties) {
  BlueZMediaFolderProps props;
  props.objectPath = folder_path;
  props.numberOfItems = media_property<uint32_t>(properties, "NumberOfItems");
  props.name = media_property<std::string>(properties, "Name");
  return glz::encode(props);
}

int MediaBrowserProxy::item_play(const std::string& item_path) const {
  media_call(*make_item_proxy(item_path), kMediaItemIface, "Play");
  return BLUEZ_MEDIA_SUCCESS;
}

int MediaBrowserProxy::item_add_to_now_playing(
    const std::string& item_path) const {
  media_call(*make_item_proxy(item_path), kMediaItemIface, "AddtoNowPlaying");
  return BLUEZ_MEDIA_SUCCESS;
}

std::vector<uint8_t> MediaBrowserProxy::item_properties(
    const std::string& item_path) const {
  auto proxy = make_item_proxy(item_path);
  std::map<std::string, sdbus::Variant> properties;
  media_call(*proxy, "org.freedesktop.DBus.Properties", "GetAll", std::string{kMediaItemIface}) >> properties;
  return encode_item_properties(item_path, properties);
}

std::vector<uint8_t> MediaBrowserProxy::encode_item_properties(
    const std::string& item_path,
    const std::map<std::string, sdbus::Variant>& properties) {
  return glz::encode(item_from_properties(item_path, properties));
}
