add_executable(order_manager_test balance_checker_test.cpp order_manager_test.cpp)

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH ORDER_MANAGER_DIR)
target_include_directories(order_manager_test PRIVATE ${ORDER_MANAGER_DIR}/src)

target_link_libraries(order_manager_test PRIVATE GTest::gmock_main order_manager_lib)

gtest_discover_tests(order_manager_test)

target_precompile_headers(order_manager_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/precompiled.h)
