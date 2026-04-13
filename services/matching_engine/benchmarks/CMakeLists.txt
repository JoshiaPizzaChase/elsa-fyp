add_executable(matching_engine_benchmark
        benchmarks.cpp
)

target_include_directories(matching_engine_benchmark
        PRIVATE
        ${CMAKE_SOURCE_DIR}/libs
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_options(matching_engine_benchmark PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(matching_engine_benchmark PRIVATE benchmark::benchmark matching_engine_lib)
