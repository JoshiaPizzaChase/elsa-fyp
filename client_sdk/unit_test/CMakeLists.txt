find_package(Catch2 3 REQUIRED)
include(Catch)

add_executable(sdk_unit_test test.cpp)

target_link_libraries(sdk_unit_test PUBLIC client_sdk)

# link to catch2 library
target_link_libraries(sdk_unit_test PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(sdk_unit_test)
