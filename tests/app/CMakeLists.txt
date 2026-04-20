add_executable(coupecad_app_version_test app_version_test.cpp)

target_link_libraries(coupecad_app_version_test
    PRIVATE
        GTest::gtest
        GTest::gtest_main
)

target_compile_definitions(coupecad_app_version_test
    PRIVATE
        COUPECAD_BINARY_PATH="$<TARGET_FILE:coupecad>"
)

add_dependencies(coupecad_app_version_test coupecad)

include(GoogleTest)
gtest_discover_tests(coupecad_app_version_test
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
