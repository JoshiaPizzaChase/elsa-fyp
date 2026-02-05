find_package(Catch2 3 REQUIRED)
include(Catch)

add_executable(gateway_unit_test test.cpp)

# find packages required for QuickFIX
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(ZLIB REQUIRED)

# Find QuickFIX via pkg-config
# pkg_check_modules(QuickFIX REQUIRED quickfix)

# Find QuickFIX in user-installed library paths
find_library(quickfix
        NAMES libquickfix
        PATHS /usr/local/lib
)

# Link QuickFIX
target_link_libraries(gateway_unit_test PUBLIC
        gateway_lib
        quickfix
        ${QuickFIX_LIBRARIES}
        Threads::Threads
        LibXml2::LibXml2
        ZLIB::ZLIB
)
target_include_directories(gateway_unit_test PUBLIC ${QuickFIX_INCLUDE_DIRS})
target_compile_definitions(gateway_unit_test PUBLIC ${QuickFIX_CFLAGS_OTHER} SPDLOG_USE_STD_FORMAT)

# link to catch2 library
target_link_libraries(gateway_unit_test PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(gateway_unit_test)
