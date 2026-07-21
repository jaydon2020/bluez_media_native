// media_utils.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <map>
#include <string>
#include <vector>

#include "bluez_media_types.h"

template <typename T>
T media_property(const std::map<std::string, sdbus::Variant>& properties,
                 const std::string& name,
                 T fallback = {}) {
  const auto it = properties.find(name);
  if (it == properties.end() || !it->second.containsValueOfType<T>()) {
    return fallback;
  }
  return it->second.get<T>();
}

std::string variant_to_string(const sdbus::Variant& value);
std::vector<BlueZMediaProperty> track_to_properties(
    const std::map<std::string, sdbus::Variant>& track);
