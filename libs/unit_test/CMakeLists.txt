cmake_minimum_required(VERSION 4.0)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Catch2 3 REQUIRED)
include(Catch)

add_executable(test_interactive_client test_interactive_client.cpp)
add_executable(test_interactive_server test_interactive_server.cpp)
add_executable(test_mpsc_producer test_mpsc_producer.cpp test_mpsc.h)
add_executable(test_mpsc_consumer test_mpsc_consumer.cpp test_mpsc.h)
add_executable(test_thread_safe_queue test_thread_safe_queue.cpp)
add_executable(test_client_server_ping_pong test_client_server_ping_pong.cpp)
add_executable(test_two_client_one_server test_two_client_one_server.cpp)

target_include_directories(test_mpsc_producer
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)

target_include_directories(test_mpsc_consumer
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)

target_include_directories(test_interactive_client
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)


target_include_directories(test_interactive_server
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)

target_include_directories(test_thread_safe_queue
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)

target_include_directories(test_client_server_ping_pong
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)

target_include_directories(test_two_client_one_server
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)

target_compile_options(test_interactive_client PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(test_interactive_server PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(test_mpsc_producer PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(test_mpsc_consumer PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(test_thread_safe_queue PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(test_two_client_one_server PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(test_client_server_ping_pong PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(test_interactive_client PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)
set_target_properties(test_interactive_server PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)
set_target_properties(test_mpsc_producer PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)
set_target_properties(test_mpsc_consumer PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)
set_target_properties(test_thread_safe_queue PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)
set_target_properties(test_client_server_ping_pong PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)
set_target_properties(test_two_client_one_server PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)

target_link_libraries(test_interactive_client PRIVATE websocket_lib)
target_link_libraries(test_interactive_server PRIVATE websocket_lib)
target_link_libraries(test_thread_safe_queue PRIVATE Catch2::Catch2WithMain)
target_link_libraries(test_client_server_ping_pong PRIVATE websocket_lib)
target_link_libraries(test_two_client_one_server PRIVATE websocket_lib)

target_compile_definitions(test_interactive_client PRIVATE SPDLOG_USE_STD_FORMAT)
target_compile_definitions(test_interactive_server PRIVATE SPDLOG_USE_STD_FORMAT)
target_compile_definitions(test_client_server_ping_pong PRIVATE SPDLOG_USE_STD_FORMAT)
target_compile_definitions(test_two_client_one_server PRIVATE SPDLOG_USE_STD_FORMAT)

catch_discover_tests(test_thread_safe_queue)
