cmake_minimum_required(VERSION 4.0)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(elsa_fyp)

if (BUILD_TESTS)
    enable_testing()
    include(CTest)
    add_subdirectory(integration_test)
endif ()

add_subdirectory(client_sdk)
add_subdirectory(services/matching_engine)
