find_package(Boost REQUIRED COMPONENTS system json)

add_executable(test_backend_service_with_mock_data
        test_backend_service_with_mock_data.cpp
)

target_include_directories(test_backend_service_with_mock_data PRIVATE
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)

target_compile_options(test_backend_service_with_mock_data PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(test_backend_service_with_mock_data PRIVATE
        GTest::gtest
        Boost::system
        Boost::json
        pqxx::pqxx
)

# Inject the path to the backend_service binary at compile time
target_compile_definitions(test_backend_service_with_mock_data PRIVATE
        BACKEND_SERVICE_BINARY="$<TARGET_FILE:backend_service>"
)

include(GoogleTest)
gtest_discover_tests(test_backend_service_with_mock_data)
