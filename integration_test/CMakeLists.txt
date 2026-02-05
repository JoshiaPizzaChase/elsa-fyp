find_package(Catch2 3 REQUIRED)
include(Catch)

add_executable(test_sdk_with_server test_sdk_with_server.cpp)
target_link_libraries(test_sdk_with_server PUBLIC client_sdk)

target_compile_definitions(test_sdk_with_server PRIVATE SPDLOG_USE_STD_FORMAT)
target_compile_definitions(test_sdk_with_server PRIVATE PROJECT_ROOT_DIR="${PROJECT_SOURCE_DIR}")

add_executable(test_end_to_end test_end_to_end.cpp)
target_link_libraries(test_end_to_end PUBLIC client_sdk)
target_compile_definitions(test_end_to_end PRIVATE SPDLOG_USE_STD_FORMAT)
target_compile_definitions(test_end_to_end PRIVATE PROJECT_ROOT_DIR="${PROJECT_SOURCE_DIR}"
        PROJECT_BUILD_DIR="${CMAKE_BINARY_DIR}")
add_dependencies(test_end_to_end client_sdk gateway order_manager matching_engine market_data_processor)

# link to catch2 library
#target_link_libraries(test_sdk_with_server PRIVATE Catch2::Catch2WithMain)
#catch_discover_tests(test_sdk_with_server)

target_link_libraries(test_end_to_end PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(test_end_to_end)
