add_executable(coupecad_logger_test
    logger_levels_test.cpp
)

target_link_libraries(coupecad_logger_test
    PRIVATE
        coupecad_logging
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_logger_test)
