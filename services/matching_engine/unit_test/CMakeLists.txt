include(Catch)

add_executable(matching_engine_test limit_order_book_test.cpp)

# link to catch2 library
target_link_libraries(matching_engine_test PRIVATE Catch2::Catch2WithMain matching_engine_lib)

catch_discover_tests(matching_engine_test)

target_precompile_headers(matching_engine_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/precompiled.h)
