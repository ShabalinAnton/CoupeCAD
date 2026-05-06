add_executable(coupecad_renderer_test
    entity_id_test.cpp
    view_driver_test.cpp
    material_resolver_test.cpp
    ais_scene_test.cpp
    occt_renderer_sync_test.cpp
)

target_link_libraries(coupecad_renderer_test
    PRIVATE
        coupecad_renderer
        coupecad_renderer_occt
        coupecad_logging
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_renderer_test)
