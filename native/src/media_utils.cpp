// media_utils.cpp
#include "media_utils.h"
#include <sstream>

std::string variant_to_string(const sdbus::Variant &value) {
  if (value.containsValueOfType<std::string>()) {
    return value.get<std::string>();
  }
  if (value.containsValueOfType<sdbus::ObjectPath>()) {
    return value.get<sdbus::ObjectPath>();
  }
  if (value.containsValueOfType<bool>()) {
    return value.get<bool>() ? "true" : "false";
  }
  if (value.containsValueOfType<uint8_t>()) {
    return std::to_string(value.get<uint8_t>());
  }
  if (value.containsValueOfType<uint16_t>()) {
    return std::to_string(value.get<uint16_t>());
  }
  if (value.containsValueOfType<uint32_t>()) {
    return std::to_string(value.get<uint32_t>());
  }
  if (value.containsValueOfType<uint64_t>()) {
    return std::to_string(value.get<uint64_t>());
  }
  if (value.containsValueOfType<int16_t>()) {
    return std::to_string(value.get<int16_t>());
  }
  if (value.containsValueOfType<int32_t>()) {
    return std::to_string(value.get<int32_t>());
  }
  if (value.containsValueOfType<int64_t>()) {
    return std::to_string(value.get<int64_t>());
  }
  if (value.containsValueOfType<double>()) {
    std::ostringstream out;
    out << value.get<double>();
    return out.str();
  }
  if (value.containsValueOfType<std::vector<std::string>>()) {
    const auto values = value.get<std::vector<std::string>>();
    std::string joined;
    for (const auto &item : values) {
      if (!joined.empty()) {
        joined += ",";
      }
      joined += item;
    }
    return joined;
  }
  return value.dumpToString();
}

std::vector<BlueZMediaProperty>
track_to_properties(const std::map<std::string, sdbus::Variant> &track) {
  std::vector<BlueZMediaProperty> properties;
  properties.reserve(track.size());
  for (const auto &[key, value] : track) {
    properties.push_back(BlueZMediaProperty{key, variant_to_string(value)});
  }
  return properties;
}
