add_executable(backend_service main.cpp)
target_compile_options(backend_service PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(backend_service
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)

find_package(Boost REQUIRED COMPONENTS system thread json)

target_link_libraries(backend_service PRIVATE Boost::system Boost::thread Boost::json reflectcpp)
