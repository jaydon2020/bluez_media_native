cmake_minimum_required(VERSION 3.10)
project(bluez_media_native_library VERSION 0.0.1 LANGUAGES C CXX)

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

add_library(bluez_media_native_core STATIC
  src/bluez_media_client.cpp
  src/bluez_media_native.c
  src/media_client.cpp
  src/local_player.cpp
  src/media_player_proxy.cpp
  src/media_control_proxy.cpp
  src/media_browser_proxy.cpp
  src/media_transport_proxy.cpp
  src/media_utils.cpp
)

target_include_directories(bluez_media_native_core
  PUBLIC include
)

target_compile_definitions(bluez_media_native_core
  PUBLIC DART_SHARED_LIB
)

target_link_libraries(bluez_media_native_core
  PUBLIC sdbus-c++ pthread
)

set_target_properties(bluez_media_native_core PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  LINKER_LANGUAGE CXX
)

add_library(bluez_media_native SHARED
  src/bluez_media_client.cpp
  src/bluez_media_native.c
  src/media_client.cpp
  src/local_player.cpp
  src/media_player_proxy.cpp
  src/media_control_proxy.cpp
  src/media_browser_proxy.cpp
  src/media_transport_proxy.cpp
  src/media_utils.cpp
)

target_include_directories(bluez_media_native
  PRIVATE include
)

target_compile_definitions(bluez_media_native
  PUBLIC DART_SHARED_LIB
)

target_link_libraries(bluez_media_native
  PRIVATE sdbus-c++ pthread
)

set_target_properties(bluez_media_native PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  LINKER_LANGUAGE CXX
  PUBLIC_HEADER include/bluez_media_native.h
  OUTPUT_NAME "bluez_media_native"
)

if(ANDROID)
  # Support Android 15 16k page size.
  target_link_options(bluez_media_native PRIVATE "-Wl,-z,max-page-size=16384")
endif()

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()
