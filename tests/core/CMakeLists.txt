add_executable(coupecad_core_test
    units_test.cpp
    id_test.cpp
    material_test.cpp
    hardware_test.cpp
    panel_test.cpp
    cabinet_test.cpp
    project_test.cpp
    geometry_test.cpp
    undo_stack_test.cpp
    undo_stack_macro_test.cpp
)

target_link_libraries(coupecad_core_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_core_test)

add_subdirectory(commands)
