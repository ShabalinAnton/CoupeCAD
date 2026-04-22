add_executable(coupecad_core_test
    units_test.cpp
    id_test.cpp
    material_test.cpp
    hardware_test.cpp
    panel_test.cpp
)

target_link_libraries(coupecad_core_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_core_test)
