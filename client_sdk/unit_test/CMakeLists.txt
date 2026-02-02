find_package(Catch2 3 REQUIRED)
include(Catch)

add_executable(sdk_unit_test test.cpp)

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
target_link_libraries(sdk_unit_test PUBLIC
        client_sdk
        quickfix
        ${QuickFIX_LIBRARIES}
        Threads::Threads
        LibXml2::LibXml2
        ZLIB::ZLIB
)
target_include_directories(sdk_unit_test PUBLIC ${QuickFIX_INCLUDE_DIRS})
target_compile_definitions(sdk_unit_test PUBLIC ${QuickFIX_CFLAGS_OTHER})
target_compile_definitions(sdk_unit_test PRIVATE SPDLOG_USE_STD_FORMAT)
target_compile_definitions(sdk_unit_test PRIVATE PROJECT_ROOT_DIR="${PROJECT_SOURCE_DIR}")

# link to catch2 library
target_link_libraries(sdk_unit_test PRIVATE Catch2::Catch2WithMain)

catch_discover_tests(sdk_unit_test)
