add_library(order_manager_lib
        ${CMAKE_CURRENT_SOURCE_DIR}/src/order_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/balance_checker.cpp
)
target_include_directories(order_manager_lib
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)
target_link_libraries(order_manager_lib PUBLIC websocket_lib reflectcpp proto-objects)
target_compile_definitions(order_manager_lib PUBLIC PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}" SPDLOG_USE_STD_FORMAT)

target_precompile_headers(order_manager_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/precompiled_lib.h)

add_executable(order_manager
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_link_libraries(order_manager PRIVATE order_manager_lib)
target_compile_options(order_manager PRIVATE -Wall -Wextra -Wpedantic)

target_precompile_headers(order_manager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/precompiled_exec.h)

if (BUILD_TESTS)
    add_subdirectory(unit_test)
endif ()
