add_library(traders_lib
        ${CMAKE_CURRENT_SOURCE_DIR}/noise_trader.cpp
)
target_include_directories(traders_lib
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
        /opt/homebrew/include
)
target_link_libraries(traders_lib PRIVATE client_sdk)
