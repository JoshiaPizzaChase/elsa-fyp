find_package(Catch2 3 REQUIRED)
include(Catch)

add_executable(order_manager_test order_manager_test.cpp balance_checker_test.cpp)

# link to catch2 library
target_link_libraries(order_manager_test PRIVATE Catch2::Catch2WithMain order_manager_lib)

target_include_directories(order_manager_test
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)

catch_discover_tests(order_manager_test)