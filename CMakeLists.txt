cmake_minimum_required(VERSION 4.0)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # This generates compile_commands.json!
set(CMAKE_BUILD_RPATH "/usr/local/lib") # This sets runtime path for dynamic linking, needed for self-compiled 3rd party libraries!
#set(CMAKE_CXX_COMPILER "/usr/bin/g++-14")
#set(CMAKE_C_COMPILER "/usr/bin/gcc-14")
project(elsa_fyp)

configure_file(
        "${PROJECT_SOURCE_DIR}/config.h.in"
        "${PROJECT_BINARY_DIR}/config.h"
)

#if(NOT TARGET spdlog)
#    find_package(spdlog REQUIRED)
#endif()

if (BUILD_TESTS)
    enable_testing()
    include(CTest)
    add_subdirectory(integration_test)
    add_subdirectory(libs/unit_test)
endif ()

if (BUILD_EXAMPLES)
    add_subdirectory(logger_examples)
endif ()

add_subdirectory(client_sdk)

add_subdirectory(libs/transport/websocket)

# Add new services here
add_subdirectory(services/matching_engine)
add_subdirectory(services/order_manager)
add_subdirectory(services/gateway)
add_subdirectory(services/market_data_processor)

# Internal libraries
# Protobuf
find_package(protobuf CONFIG REQUIRED) # Having the keyword CONFIG is IMPORTANT to avoid abseil errors!!!!!

### Generates header files and cpp files automatically instead of running cli command ourselves.
set(PROTO_BINARY_DIR "${CMAKE_SOURCE_DIR}/libs/transport/protobufs")
add_library(proto-objects OBJECT "${PROTO_BINARY_DIR}/containers.proto")
target_link_libraries(proto-objects PUBLIC protobuf::libprotobuf)
protobuf_generate(
        TARGET proto-objects
        IMPORT_DIRS "${PROTO_BINARY_DIR}"
        PROTOC_OUT_DIR "${PROTO_BINARY_DIR}")
