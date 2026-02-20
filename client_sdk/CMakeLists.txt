add_library(client_sdk
        src/fix_client.cpp
)
target_include_directories(client_sdk
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_options(client_sdk PRIVATE -Wall -Wextra -Wpedantic -finline-functions)

# Link QuickFIX
target_link_libraries(client_sdk PUBLIC
        quickfix
        ${QuickFIX_LIBRARIES}
        Threads::Threads
        LibXml2::LibXml2
        ZLIB::ZLIB
)
target_include_directories(client_sdk PUBLIC ${QuickFIX_INCLUDE_DIRS} ${QuickFIX_CFLAGS_OTHER})

target_compile_definitions(client_sdk PUBLIC SPDLOG_USE_STD_FORMAT)
target_compile_definitions(client_sdk PUBLIC PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# Handling precompile headers
set(CLIENT_SDK_PCH ${CMAKE_CURRENT_SOURCE_DIR}/precompiled.h)
target_precompile_headers(client_sdk PRIVATE ${CLIENT_SDK_PCH})

if (BUILD_TESTS)
    add_subdirectory(unit_test)
endif ()
