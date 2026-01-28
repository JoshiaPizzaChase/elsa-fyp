find_package(Catch2 3 REQUIRED)
include(Catch)

add_executable(test_sdk_with_server test_sdk_with_server.cpp
        test_sdk_with_server.cpp)
target_link_libraries(test_sdk_with_server PUBLIC client_sdk fmt)

# link to catch2 library
#target_link_libraries(test_sdk_with_server PRIVATE Catch2::Catch2WithMain)

#catch_discover_tests(test_sdk_with_server)
