add_executable(test_noise_trader test_noise_trader.cpp)
add_executable(test_market_maker test_market_maker.cpp)
add_executable(test_informed_trader test_informed_trader.cpp)
add_executable(test_oracle test_oracle.cpp)

target_include_directories(test_noise_trader PRIVATE
    ${CMAKE_SOURCE_DIR}/simulation/traders
    ${CMAKE_SOURCE_DIR}/client_sdk/include
)

target_include_directories(test_market_maker PRIVATE
    ${CMAKE_SOURCE_DIR}/simulation/traders
    ${CMAKE_SOURCE_DIR}/client_sdk/include
    ${CMAKE_SOURCE_DIR}/libs
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    /opt/homebrew/include
)

target_include_directories(test_informed_trader PRIVATE
    ${CMAKE_SOURCE_DIR}/simulation/traders
    ${CMAKE_SOURCE_DIR}/client_sdk/include
    ${CMAKE_SOURCE_DIR}/libs
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    /opt/homebrew/include
)

target_include_directories(test_oracle PRIVATE
    ${CMAKE_SOURCE_DIR}/simulation/traders
    ${CMAKE_SOURCE_DIR}/client_sdk/include
    ${CMAKE_SOURCE_DIR}/libs
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    /opt/homebrew/include
)

target_link_libraries(test_noise_trader PRIVATE
    client_sdk
    gtest
    gtest_main
    Threads::Threads
)

target_link_libraries(test_market_maker PRIVATE
    client_sdk
    websocket_lib
    gtest
    gtest_main
    Threads::Threads
)

target_link_libraries(test_informed_trader PRIVATE
    client_sdk
    websocket_lib
    gtest
    gtest_main
    Threads::Threads
)

target_link_libraries(test_oracle PRIVATE
    websocket_lib
    gtest
    gtest_main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(test_noise_trader)
gtest_discover_tests(test_market_maker)
gtest_discover_tests(test_informed_trader)
gtest_discover_tests(test_oracle)
