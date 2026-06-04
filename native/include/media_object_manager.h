#pragma once

#include <sdbus-c++/sdbus-c++.h>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "dart_api_dl.h"

class MediaObjectManager {
public:
  MediaObjectManager(sdbus::IConnection &conn, Dart_Port_DL events_port);
  ~MediaObjectManager();

  void get_managed_objects();

private:
  using InterfacesMap =
      std::map<std::string, std::map<std::string, sdbus::Variant>>;

  void on_interfaces_added(const sdbus::ObjectPath &object_path,
                           const InterfacesMap &interfaces);
  void on_interfaces_removed(const sdbus::ObjectPath &object_path,
                             const std::vector<std::string> &interfaces);
  void subscribe_properties(const std::string &object_path);
  void post_properties(const std::string &object_path,
                       const std::string &interface_name);
  void post_removed(const std::string &object_path,
                    const std::string &interface_name);

  sdbus::IConnection &conn_;
  Dart_Port_DL events_port_;
  std::unique_ptr<sdbus::IProxy> root_proxy_;
  std::map<std::string, std::unique_ptr<sdbus::IProxy>> property_proxies_;
  std::map<std::string, std::set<std::string>> interfaces_by_path_;
};
