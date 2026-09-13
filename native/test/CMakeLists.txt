add_executable(test_media_types test_media_types.cpp $<TARGET_OBJECTS:dart_api_dl>)
target_link_libraries(test_media_types PRIVATE bluez_media_native_core)

add_test(NAME test_media_types COMMAND test_media_types)

add_executable(test_media_lifetime test_media_lifetime.cpp $<TARGET_OBJECTS:dart_api_dl>)
target_link_libraries(test_media_lifetime PRIVATE bluez_media_native_core)
# Assertions execute the checks even in a Release test build.
target_compile_options(test_media_lifetime PRIVATE -UNDEBUG)
add_test(NAME test_media_lifetime COMMAND test_media_lifetime)
set_tests_properties(test_media_lifetime PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
