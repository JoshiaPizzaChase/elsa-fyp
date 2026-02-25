include(Catch)

add_executable(test_end_to_end test_end_to_end.cpp)
target_link_libraries(test_end_to_end PRIVATE client_sdk)
target_compile_definitions(test_end_to_end PRIVATE SPDLOG_USE_STD_FORMAT)
target_compile_definitions(test_end_to_end PRIVATE PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        PROJECT_BUILD_DIR="${CMAKE_BINARY_DIR}")
add_dependencies(test_end_to_end gateway order_manager matching_engine market_data_processor)

target_link_libraries(test_end_to_end PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(test_end_to_end)

target_precompile_headers(test_end_to_end PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/precompiled.h)
