add_library(market_data_processor_lib
        ${CMAKE_CURRENT_SOURCE_DIR}/src/market_data_processor.cpp
)
target_include_directories(market_data_processor_lib
        PUBLIC
        ${CMAKE_SOURCE_DIR}/libs
        /opt/homebrew/include
)
target_link_libraries(market_data_processor_lib PUBLIC websocket_lib nlohmann_json::nlohmann_json reflectcpp)
target_compile_definitions(market_data_processor_lib PUBLIC SPDLOG_USE_STD_FORMAT)
target_compile_definitions(market_data_processor_lib PUBLIC PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

target_precompile_headers(market_data_processor_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/precompiled_lib.h)

add_executable(market_data_processor
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_link_libraries(market_data_processor PRIVATE market_data_processor_lib)
target_compile_options(market_data_processor PRIVATE -Wall -Wextra -Wpedantic)

target_precompile_headers(market_data_processor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/precompiled_exec.h)

if (BUILD_TESTS)
    add_subdirectory(unit_test)
endif ()
