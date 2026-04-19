add_executable(coupecad_smoke_test smoke_test.cpp)

target_link_libraries(coupecad_smoke_test
    PRIVATE
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_smoke_test)
