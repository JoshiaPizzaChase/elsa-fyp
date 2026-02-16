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

target_link_libraries(test_md_producer PRIVATE)
target_link_libraries(test_mdp_client PRIVATE websocket_lib)
