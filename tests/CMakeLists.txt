# Tests CMakeLists.txt

add_executable(unit_tests
    test_core.cpp
    test_platform.cpp
    test_android_auto.cpp
    test_command_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/command_parser.cpp
)

target_include_directories(unit_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../examples
)

target_link_libraries(unit_tests
    PRIVATE
        phone_projection_middleware
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME unit_tests COMMAND unit_tests)

# Integration tests
add_executable(integration_tests
    test_integration.cpp
)

target_include_directories(integration_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(integration_tests
    PRIVATE
        phone_projection_middleware
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME integration_tests COMMAND integration_tests)