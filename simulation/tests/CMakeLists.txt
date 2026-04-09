add_executable(test_noise_trader test_noise_trader.cpp)

target_include_directories(test_noise_trader PRIVATE
    ${CMAKE_SOURCE_DIR}/simulation/traders
    ${CMAKE_SOURCE_DIR}/client_sdk/include
)

target_link_libraries(test_noise_trader PRIVATE
    # traders_lib
    client_sdk
    gtest
    gtest_main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(test_noise_trader)
