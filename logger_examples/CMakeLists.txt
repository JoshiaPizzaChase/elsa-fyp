add_executable(logger_example logger_example.cpp)

# link to catch2 library
target_link_libraries(logger_example PRIVATE spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)