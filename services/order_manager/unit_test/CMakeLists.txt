find_package(Catch2 3 REQUIRED)
include(Catch)

add_executable(order_manager_test order_manager_test.cpp balance_checker_test.cpp)

# link to catch2 library
target_link_libraries(order_manager_test PRIVATE Catch2::Catch2WithMain order_manager_lib)

catch_discover_tests(order_manager_test)

target_precompile_headers(order_manager_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/precompiled.h)
