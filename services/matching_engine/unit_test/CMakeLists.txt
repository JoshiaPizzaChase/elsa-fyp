find_package(Catch2 3 REQUIRED)
include(Catch)

add_executable(matching_engine_test limit_order_book_test.cpp)

# link to catch2 library
target_link_libraries(matching_engine_test PRIVATE Catch2::Catch2WithMain matching_engine_lib)
target_compile_definitions(matching_engine_test PRIVATE SPDLOG_USE_STD_FORMAT)

catch_discover_tests(matching_engine_test)
