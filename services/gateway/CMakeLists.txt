cmake_minimum_required(VERSION 4.0)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(gateway
        src/gateway_application.cpp
)
target_include_directories(gateway
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)

target_compile_options(gateway PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(gateway PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)
# find packages required for QuickFIX
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(ZLIB REQUIRED)


# Find QuickFIX via pkg-confij
# pkg_check_modules(QuickFIX REQUIRED quickfix)

# Find QuickFIX in user-installed library paths
find_library(quickfix
        NAMES libquickfix
        PATHS /usr/local/lib
)

# Link QuickFIX & matching engine
target_link_libraries(gateway PUBLIC
        quickfix
        matching_engine_lib
        ${QuickFIX_LIBRARIES}
        Threads::Threads
        LibXml2::LibXml2
        ZLIB::ZLIB
        fmt
)
target_include_directories(gateway PUBLIC ${QuickFIX_INCLUDE_DIRS})
target_compile_definitions(gateway PUBLIC ${QuickFIX_CFLAGS_OTHER})

if (BUILD_TESTS)
    add_subdirectory(unit_test)
endif ()
