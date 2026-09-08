#pragma once

#include <sdbus-c++/sdbus-c++.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "dart_api_dl.h"

class MediaObjectManager {
 public:
  using RemovedCallback = std::function<void(const std::string&, const std::string&)>;
  MediaObjectManager(sdbus::IConnection& conn, Dart_Port_DL events_port,
                     RemovedCallback removed = {});
  ~MediaObjectManager();

  void get_managed_objects();

 private:
  using InterfacesMap =
      std::map<std::string, std::map<std::string, sdbus::Variant>>;

  void refresh_properties(const std::string& path, const std::string& interface_name,
                          uint64_t revision);
  void apply_update(std::function<void()> update);

  void on_interfaces_added(const sdbus::ObjectPath& object_path,
                           const InterfacesMap& interfaces);
  void on_interfaces_removed(const sdbus::ObjectPath& object_path,
                             const std::vector<std::string>& interfaces);
  void post_properties(const std::string& object_path,
                       const std::string& interface_name);
  void post_removed(const std::string& object_path,
                    const std::string& interface_name);

  sdbus::IConnection& conn_;
  Dart_Port_DL events_port_;
  RemovedCallback removed_;
  std::unique_ptr<sdbus::IProxy> root_proxy_;
  sdbus::Slot owner_subscription_;
  uint64_t owner_generation_ = 0;
  bool resynchronizing_ = false;
  std::vector<std::function<void()>> pending_updates_;
  sdbus::Slot properties_subscription_;
  std::map<std::string, std::unique_ptr<sdbus::IProxy>> property_proxies_;
  std::map<std::string, std::map<std::string, uint64_t>> revisions_;
  uint64_t next_revision_ = 0;
  std::map<std::string, std::set<std::string>> interfaces_by_path_;
  std::map<std::string, InterfacesMap> properties_by_path_;
};
