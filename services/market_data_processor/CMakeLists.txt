add_executable(market_data_processor
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

target_include_directories(market_data_processor
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)

target_compile_options(market_data_processor PRIVATE -Wall -Wextra -Wpedantic)

set_target_properties(market_data_processor PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
)