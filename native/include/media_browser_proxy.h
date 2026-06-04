// media_browser_proxy.h
#pragma once

#include "bluez_media_types.h"
#include <cstdint>
#include <map>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <vector>

class MediaBrowserProxy {
public:
  MediaBrowserProxy(sdbus::IConnection &conn);

  std::vector<uint8_t> folder_search(const std::string &folder_path,
                                     const std::string &value) const;
  std::vector<uint8_t> folder_list_items(const std::string &folder_path) const;
  int folder_change_folder(const std::string &folder_path,
                           const std::string &target_folder_path) const;
  std::vector<uint8_t> folder_properties(const std::string &folder_path) const;

  int item_play(const std::string &item_path) const;
  int item_add_to_now_playing(const std::string &item_path) const;
  std::vector<uint8_t> item_properties(const std::string &item_path) const;

private:
  static constexpr auto kBluezService = "org.bluez";
  static constexpr auto kMediaFolderIface = "org.bluez.MediaFolder1";
  static constexpr auto kMediaItemIface = "org.bluez.MediaItem1";

  std::unique_ptr<sdbus::IProxy>
  make_folder_proxy(const std::string &folder_path) const;
  std::unique_ptr<sdbus::IProxy>
  make_item_proxy(const std::string &item_path) const;

  static BlueZMediaItemProps
  item_from_properties(const std::string &object_path,
                       const std::map<std::string, sdbus::Variant> &props);

  sdbus::IConnection &conn_;
};
