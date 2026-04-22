add_executable(coupecad_io_test
    serializer_interface_test.cpp
    json_primitives_test.cpp
    json_entities_test.cpp
    json_panel_test.cpp
    json_project_test.cpp
    schema_migration_test.cpp
    ccad_archive_test.cpp
)

target_link_libraries(coupecad_io_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_io_test)
