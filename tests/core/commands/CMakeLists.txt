add_executable(coupecad_commands_test
    command_interface_test.cpp
    cabinet_commands_test.cpp
    panel_commands_test.cpp
    material_commands_test.cpp
)

target_link_libraries(coupecad_commands_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_commands_test)
