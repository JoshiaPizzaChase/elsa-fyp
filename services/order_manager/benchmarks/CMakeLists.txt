add_executable(order_manager_benchmark
        benchmarks.cpp
)

target_include_directories(order_manager_benchmark
        PRIVATE
        ${CMAKE_SOURCE_DIR}/libs
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_options(order_manager_benchmark PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(order_manager_benchmark PRIVATE benchmark::benchmark order_manager_lib)
