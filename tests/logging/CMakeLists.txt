add_executable(coupecad_logger_test
    logger_levels_test.cpp
    logger_sinks_test.cpp
    logger_file_rotation_test.cpp
)

target_link_libraries(coupecad_logger_test
    PRIVATE
        coupecad_logging
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_logger_test)
