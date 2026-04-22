add_executable(coupecad_io_test
    serializer_interface_test.cpp
    json_primitives_test.cpp
)

target_link_libraries(coupecad_io_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_io_test)
