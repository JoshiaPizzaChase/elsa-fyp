add_executable(mpsc_ring_buffer_benchmark
        mpsc_ring_buffer_benchmark.cpp
)

add_executable(websocket_benchmark
        websocket_benchmark.cpp
)

target_include_directories(mpsc_ring_buffer_benchmark
        PRIVATE
        ${CMAKE_SOURCE_DIR}/libs
)

target_include_directories(websocket_benchmark
        PRIVATE
        ${CMAKE_SOURCE_DIR}/libs
)

target_compile_options(mpsc_ring_buffer_benchmark PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(websocket_benchmark PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(mpsc_ring_buffer_benchmark PRIVATE benchmark::benchmark)
target_link_libraries(websocket_benchmark PRIVATE benchmark::benchmark websocket_lib)
