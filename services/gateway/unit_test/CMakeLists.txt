include(Catch)

add_executable(gateway_unit_test test.cpp)

# Link QuickFIX
target_link_libraries(gateway_unit_test PRIVATE gateway_lib)

# link to catch2 library
target_link_libraries(gateway_unit_test PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(gateway_unit_test)

target_precompile_headers(gateway_unit_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/precompiled.h)
