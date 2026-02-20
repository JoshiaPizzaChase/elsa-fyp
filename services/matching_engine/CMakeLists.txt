add_library(matching_engine_lib
        ${CMAKE_CURRENT_SOURCE_DIR}/src/matching_engine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/limit_order_book.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/order.cpp
)
target_include_directories(matching_engine_lib
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)
target_link_libraries(matching_engine_lib PUBLIC websocket_lib proto-objects reflectcpp)
target_compile_definitions(matching_engine_lib PUBLIC PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}" SPDLOG_USE_STD_FORMAT)

target_precompile_headers(matching_engine_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/precompiled_lib.h)

add_executable(matching_engine
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_link_libraries(matching_engine PRIVATE matching_engine_lib)

target_compile_options(matching_engine PRIVATE -Wall -Wextra -Wpedantic)

target_precompile_headers(matching_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/precompiled_exec.h)

if (BUILD_TESTS)
    add_subdirectory(unit_test)
endif ()
