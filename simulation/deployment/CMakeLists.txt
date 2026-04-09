add_executable(create_n_noise_traders create_n_noise_traders.cpp)

target_include_directories(create_n_noise_traders PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/configuration
    ${CMAKE_SOURCE_DIR}/simulation/traders
    ${CMAKE_SOURCE_DIR}/client_sdk/include
)

target_link_libraries(create_n_noise_traders PRIVATE
    client_sdk
    Threads::Threads
    reflectcpp
)
