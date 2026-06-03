// media_utils.h
#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <map>
#include <string>
#include <vector>

#include "bluez_media_types.h"

std::string variant_to_string(const sdbus::Variant &value);
std::vector<BlueZMediaProperty> track_to_properties(const std::map<std::string, sdbus::Variant> &track);
