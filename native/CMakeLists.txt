cmake_minimum_required(VERSION 3.22)
project(bluez_media_native_library VERSION 0.2.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDBUSCPP_BUILD_CODEGEN OFF CACHE BOOL "" FORCE)
set(SDBUSCPP_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(SDBUSCPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDBUSCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SDBUSCPP_INSTALL OFF CACHE BOOL "" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(third_party/sdbus-cpp EXCLUDE_FROM_ALL)

# Compile the Dart dynamic-linking shim once as plain C and reuse it in both
# native targets.
add_library(dart_api_dl OBJECT src/dart_api_dl.c)
target_include_directories(dart_api_dl PUBLIC include)
set_target_properties(dart_api_dl PROPERTIES C_VISIBILITY_PRESET hidden)

add_library(bluez_media_native_core OBJECT
  src/bluez_media_client.cpp
  src/media_client.cpp
  src/media_object_manager.cpp
  src/local_player.cpp
  src/media_player_proxy.cpp
  src/media_control_proxy.cpp
  src/media_browser_proxy.cpp
  src/media_transport_proxy.cpp
  src/cover_art_service.cpp
  src/media_utils.cpp
)

target_include_directories(bluez_media_native_core
  PUBLIC include
)

target_link_libraries(bluez_media_native_core
  PUBLIC sdbus-c++ pthread ${CMAKE_DL_LIBS}
)

set_target_properties(bluez_media_native_core PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  C_VISIBILITY_PRESET hidden
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
  LINKER_LANGUAGE CXX
)

add_library(bluez_media_native SHARED
  $<TARGET_OBJECTS:dart_api_dl>
)
target_link_libraries(bluez_media_native PRIVATE bluez_media_native_core)

set_target_properties(bluez_media_native PROPERTIES
  C_VISIBILITY_PRESET hidden
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
  LINKER_LANGUAGE CXX
  PUBLIC_HEADER include/bluez_media_native.h
  OUTPUT_NAME "bluez_media_native"
)

if(UNIX AND NOT APPLE)
  target_link_options(bluez_media_native PRIVATE "-Wl,--exclude-libs,ALL")
endif()

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()
