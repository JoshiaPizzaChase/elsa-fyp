cmake_minimum_required(VERSION 4.0)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(order_manager_ws_client_test test_client.cpp)
add_executable(order_manager_ws_server_test test_server.cpp)
add_executable(test_mpsc_producer test_mpsc_producer.cpp test_mpsc.h)
add_executable(test_mpsc_consumer test_mpsc_consumer.cpp test_mpsc.h)
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

target_include_directories(order_manager_ws_client_test
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)


target_include_directories(order_manager_ws_server_test
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)

target_compile_options(order_manager_ws_client_test PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(order_manager_ws_server_test PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(test_mpsc_producer PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(test_mpsc_consumer PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(order_manager_ws_client_test PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)
set_target_properties(order_manager_ws_server_test PROPERTIES
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

target_link_libraries(order_manager_ws_client_test PRIVATE fmt)
target_link_libraries(order_manager_ws_server_test PRIVATE fmt)
