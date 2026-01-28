cmake_minimum_required(VERSION 4.0)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(test_md_producer test_md_producer.cpp)
add_executable(test_mdp_client test_mdp_client.cpp)
target_include_directories(test_md_producer
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)

target_include_directories(test_mdp_client
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)

target_compile_options(test_md_producer PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(test_mdp_client PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(test_md_producer PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)
set_target_properties(test_mdp_client PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)

