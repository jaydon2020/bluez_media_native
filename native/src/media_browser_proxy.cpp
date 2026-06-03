// media_browser_proxy.cpp
#include "media_browser_proxy.h"
#include "media_utils.h"

MediaBrowserProxy::MediaBrowserProxy(sdbus::IConnection &conn) : conn_(conn) {}

std::unique_ptr<sdbus::IProxy> MediaBrowserProxy::make_folder_proxy(const std::string &folder_path) const {
  if (folder_path.empty()) {
    throw sdbus::Error{sdbus::Error::Name{"org.bluez.Error.InvalidArguments"},
                       "folder_path is required"};
  }
  return sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                            sdbus::ObjectPath{folder_path});
}

std::unique_ptr<sdbus::IProxy> MediaBrowserProxy::make_item_proxy(const std::string &item_path) const {
  if (item_path.empty()) {
    throw sdbus::Error{sdbus::Error::Name{"org.bluez.Error.InvalidArguments"},
                       "item_path is required"};
  }
  return sdbus::createProxy(conn_, sdbus::ServiceName{kBluezService},
                            sdbus::ObjectPath{item_path});
}

BlueZMediaItemProps MediaBrowserProxy::item_from_properties(
    const std::string &object_path,
    const std::map<std::string, sdbus::Variant> &props) {
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

std::vector<uint8_t> MediaBrowserProxy::folder_search(const std::string &folder_path, const std::string &value) const {
  auto proxy = make_folder_proxy(folder_path);
  sdbus::ObjectPath result;
  proxy->callMethod("Search")
      .onInterface(kMediaFolderIface)
      .withArguments(value, std::map<std::string, sdbus::Variant>{})
      .storeResultsTo(result);

  BlueZMediaFolderProps props;
  props.objectPath = result;
  return glz::encode(props);
}

std::vector<uint8_t> MediaBrowserProxy::folder_list_items(const std::string &folder_path) const {
  auto proxy = make_folder_proxy(folder_path);
  std::map<sdbus::ObjectPath, std::map<std::string, sdbus::Variant>> result;
  proxy->callMethod("ListItems")
      .onInterface(kMediaFolderIface)
      .withArguments(std::map<std::string, sdbus::Variant>{})
      .storeResultsTo(result);

  BlueZMediaFolderItems items;
  items.objectPath = folder_path;
  items.items.reserve(result.size());
  for (const auto &[path, props] : result) {
    items.items.push_back(item_from_properties(path, props));
  }
  return glz::encode(items);
}

int MediaBrowserProxy::folder_change_folder(const std::string &folder_path, const std::string &target_folder_path) const {
  make_folder_proxy(folder_path)
      ->callMethod("ChangeFolder")
      .onInterface(kMediaFolderIface)
      .withArguments(sdbus::ObjectPath{target_folder_path});
  return 0;
}

std::vector<uint8_t> MediaBrowserProxy::folder_properties(const std::string &folder_path) const {
  auto proxy = make_folder_proxy(folder_path);
  BlueZMediaFolderProps props;
  props.objectPath = folder_path;
  props.numberOfItems = proxy->getProperty("NumberOfItems")
                            .onInterface(kMediaFolderIface)
                            .get<uint32_t>();
  props.name = proxy->getProperty("Name")
                   .onInterface(kMediaFolderIface)
                   .get<std::string>();
  return glz::encode(props);
}

int MediaBrowserProxy::item_play(const std::string &item_path) const {
  make_item_proxy(item_path)->callMethod("Play").onInterface(kMediaItemIface);
  return 0;
}

int MediaBrowserProxy::item_add_to_now_playing(const std::string &item_path) const {
  make_item_proxy(item_path)
      ->callMethod("AddtoNowPlaying")
      .onInterface(kMediaItemIface);
  return 0;
}

std::vector<uint8_t> MediaBrowserProxy::item_properties(const std::string &item_path) const {
  auto proxy = make_item_proxy(item_path);
  BlueZMediaItemProps props;
  props.objectPath = item_path;
  props.player = proxy->getProperty("Player")
                     .onInterface(kMediaItemIface)
                     .get<sdbus::ObjectPath>();
  props.name = proxy->getProperty("Name")
                   .onInterface(kMediaItemIface)
                   .get<std::string>();
  props.type = proxy->getProperty("Type")
                   .onInterface(kMediaItemIface)
                   .get<std::string>();
  props.folderType = proxy->getProperty("FolderType")
                         .onInterface(kMediaItemIface)
                         .get<std::string>();
  props.playable =
      proxy->getProperty("Playable").onInterface(kMediaItemIface).get<bool>();
  props.metadata =
      track_to_properties(proxy->getProperty("Metadata")
                              .onInterface(kMediaItemIface)
                              .get<std::map<std::string, sdbus::Variant>>());
  return glz::encode(props);
}
