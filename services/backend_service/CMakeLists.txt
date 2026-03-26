find_package(Boost REQUIRED COMPONENTS thread json url)

add_library(backend_service_lib STATIC request_handler.cpp)
target_compile_options(backend_service_lib PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(backend_service_lib
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
        /opt/homebrew/include
        ${CMAKE_SOURCE_DIR}/deps/c-questdb-client/include
)
target_link_libraries(backend_service_lib PUBLIC
        Boost::thread Boost::json Boost::url
        reflectcpp questdb_client pqxx::pqxx
)

add_executable(backend_service main.cpp)
target_compile_options(backend_service PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(backend_service PRIVATE backend_service_lib reflectcpp)

if (BUILD_TESTS)
    add_subdirectory(unit_test)
endif ()