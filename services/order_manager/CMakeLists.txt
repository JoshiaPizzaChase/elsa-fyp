add_executable(order_manager
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

add_library(order_manager_lib
        ${CMAKE_CURRENT_SOURCE_DIR}/src/order_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/balance_checker.cpp
)

target_include_directories(order_manager_lib
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)

target_link_libraries(order_manager PRIVATE order_manager_lib)

target_compile_options(order_manager PRIVATE -Wall -Wextra -Wpedantic)

target_compile_definitions(order_manager PRIVATE SPDLOG_USE_STD_FORMAT)
set_target_properties(order_manager PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)

if (BUILD_TESTS)
    add_subdirectory(unit_test)
endif ()
