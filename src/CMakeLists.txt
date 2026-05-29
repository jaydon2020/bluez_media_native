# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)

project(bluez_media_native_library VERSION 0.0.1 LANGUAGES C)

add_library(bluez_media_native SHARED
  "bluez_media_native.c"
)

set_target_properties(bluez_media_native PROPERTIES
  PUBLIC_HEADER bluez_media_native.h
  OUTPUT_NAME "bluez_media_native"
)

target_compile_definitions(bluez_media_native PUBLIC DART_SHARED_LIB)

if (ANDROID)
  # Support Android 15 16k page size
  target_link_options(bluez_media_native PRIVATE "-Wl,-z,max-page-size=16384")
endif()
