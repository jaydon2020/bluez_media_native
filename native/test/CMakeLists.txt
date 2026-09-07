add_executable(test_media_types test_media_types.cpp $<TARGET_OBJECTS:dart_api_dl>)
target_link_libraries(test_media_types PRIVATE bluez_media_native_core)

add_test(NAME test_media_types COMMAND test_media_types)
