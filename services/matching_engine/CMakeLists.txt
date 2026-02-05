add_executable(matching_engine
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

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
target_compile_definitions(matching_engine_lib PUBLIC PROJECT_ROOT_DIR="${PROJECT_SOURCE_DIR}")

target_link_libraries(matching_engine PRIVATE matching_engine_lib)

target_compile_options(matching_engine PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(matching_engine PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)

if (BUILD_TESTS)
    add_subdirectory(unit_test)
endif ()
