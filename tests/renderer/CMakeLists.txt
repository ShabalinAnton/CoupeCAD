add_executable(coupecad_renderer_test
    entity_id_test.cpp
)

target_link_libraries(coupecad_renderer_test
    PRIVATE
        coupecad_renderer
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_renderer_test)
