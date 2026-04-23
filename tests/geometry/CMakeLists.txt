add_executable(coupecad_geometry_test
    occt_helpers_test.cpp
    panel_shape_test.cpp
)

target_link_libraries(coupecad_geometry_test
    PRIVATE
        coupecad_geometry
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_geometry_test)
