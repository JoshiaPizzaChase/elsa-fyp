add_executable(market_data_processor
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
add_library(market_data_processor_lib
        ${CMAKE_CURRENT_SOURCE_DIR}/src/market_data_processor.cpp
)
target_include_directories(market_data_processor_lib
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)
find_package(nlohmann_json REQUIRED)
target_link_libraries(market_data_processor PRIVATE market_data_processor_lib)
target_link_libraries(market_data_processor PRIVATE nlohmann_json::nlohmann_json fmt)
target_compile_options(market_data_processor PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(market_data_processor PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)

if (BUILD_TESTS)
    add_subdirectory(unit_test)
endif ()
