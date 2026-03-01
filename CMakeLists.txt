cmake_minimum_required(VERSION 4.0)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # This generates compile_commands.json!
set(CMAKE_BUILD_RPATH "/usr/local/lib") # This sets runtime path for dynamic linking, needed for self-compiled 3rd party libraries!

project(elsa_fyp)

# For injection of build information at build time
configure_file(
        "${PROJECT_SOURCE_DIR}/config.h.in"
        "${PROJECT_BINARY_DIR}/config.h"
)

# External Dependencies
# QuickFIX
find_package(Threads REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(ZLIB REQUIRED)
find_library(quickfix
        NAMES libquickfix
        PATHS /usr/local/lib
)

# Protobuf
find_package(protobuf CONFIG REQUIRED) # Having the keyword CONFIG is IMPORTANT to avoid abseil errors!!!!!

# Generates header files and cpp files automatically instead of running cli command ourselves.
set(PROTO_BINARY_DIR "${CMAKE_SOURCE_DIR}/libs/transport/protobufs")
add_library(proto-objects OBJECT "${PROTO_BINARY_DIR}/containers.proto")
target_link_libraries(proto-objects PUBLIC protobuf::libprotobuf)
protobuf_generate(
        TARGET proto-objects
        IMPORT_DIRS "${PROTO_BINARY_DIR}"
        PROTOC_OUT_DIR "${PROTO_BINARY_DIR}")

# Reflectcpp
find_package(reflectcpp REQUIRED)

# Spdlog
#if(NOT TARGET spdlog)
#    find_package(spdlog REQUIRED)
#endif()
if (BUILD_EXAMPLES)
    add_subdirectory(logger_examples)
endif ()

# JSON
find_package(nlohmann_json REQUIRED)

# Catch 2
if (BUILD_TESTS)
    find_package(Catch2 3 REQUIRED)
endif ()

# Building our code
if (BUILD_TESTS)
    enable_testing()
    include(CTest)
    add_subdirectory(integration_test)
    add_subdirectory(libs/unit_test)
endif ()

add_subdirectory(client_sdk)

add_subdirectory(libs/transport/websocket)

# Add new services here
add_subdirectory(services/gateway)
add_subdirectory(services/order_manager)
add_subdirectory(services/matching_engine)
add_subdirectory(services/market_data_processor)


