add_executable(mpsc_ring_buffer_benchmark
        benchmarks.cpp
)

target_include_directories(mpsc_ring_buffer_benchmark
        PRIVATE
        ${CMAKE_SOURCE_DIR}/libs
)

target_compile_options(mpsc_ring_buffer_benchmark PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(mpsc_ring_buffer_benchmark PRIVATE benchmark::benchmark)
