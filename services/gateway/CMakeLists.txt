add_library(gateway_lib
        src/gateway_application.cpp
)
target_include_directories(gateway_lib
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)

target_compile_options(gateway_lib PRIVATE -Wall -Wextra -Wpedantic)

# Link QuickFIX
target_link_libraries(gateway_lib PUBLIC
        quickfix
        ${QuickFIX_LIBRARIES}
        Threads::Threads
        LibXml2::LibXml2
        ZLIB::ZLIB
        websocket_lib
        proto-objects
        reflectcpp
)

target_include_directories(gateway_lib PUBLIC ${QuickFIX_INCLUDE_DIRS})
target_compile_definitions(gateway_lib PUBLIC ${QuickFIX_CFLAGS_OTHER} SPDLOG_USE_STD_FORMAT)
target_compile_definitions(gateway_lib PUBLIC PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

add_executable(gateway
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

target_link_libraries(gateway PRIVATE gateway_lib)

target_compile_options(gateway PRIVATE -Wall -Wextra -Wpedantic)

if (BUILD_TESTS)
    add_subdirectory(unit_test)
endif ()
