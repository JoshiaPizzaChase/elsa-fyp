# Noise Traders
add_executable(create_n_noise_traders create_n_noise_traders.cpp)
target_include_directories(create_n_noise_traders PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/configuration
    ${CMAKE_SOURCE_DIR}/simulation/traders
    ${CMAKE_SOURCE_DIR}/client_sdk/include
    ${CMAKE_SOURCE_DIR}/libs
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    /opt/homebrew/include
)
target_link_libraries(create_n_noise_traders PRIVATE
    client_sdk
    Threads::Threads
    reflectcpp
)

# Market Makers
add_executable(create_n_market_makers create_n_market_makers.cpp)
target_include_directories(create_n_market_makers PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/configuration
    ${CMAKE_SOURCE_DIR}/simulation/traders
    ${CMAKE_SOURCE_DIR}/client_sdk/include
    ${CMAKE_SOURCE_DIR}/libs
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    /opt/homebrew/include
)

target_link_libraries(create_n_market_makers PRIVATE
    client_sdk
    websocket_lib
    Threads::Threads
    reflectcpp
)

# Informed Traders
add_executable(create_n_informed_traders create_n_informed_traders.cpp)
target_include_directories(create_n_informed_traders PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/configuration
    ${CMAKE_SOURCE_DIR}/simulation/traders
    ${CMAKE_SOURCE_DIR}/client_sdk/include
    ${CMAKE_SOURCE_DIR}/libs
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    /opt/homebrew/include
)
target_link_libraries(create_n_informed_traders PRIVATE
    client_sdk
    websocket_lib
    Threads::Threads
    reflectcpp
)

# Oracle
add_executable(create_oracle create_oracle.cpp)
target_include_directories(create_oracle PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/configuration
    ${CMAKE_SOURCE_DIR}/simulation/traders
    ${CMAKE_SOURCE_DIR}/client_sdk/include
    ${CMAKE_SOURCE_DIR}/libs
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    /opt/homebrew/include
)
target_link_libraries(create_oracle PRIVATE
    client_sdk
    websocket_lib
    Threads::Threads
    reflectcpp
)
