find_package(Qt6 6.5 REQUIRED COMPONENTS Core Test)

add_executable(coupecad_ui_test
    undo_stack_proxy_test.cpp
)

target_link_libraries(coupecad_ui_test
    PRIVATE
        coupecad_ui
        coupecad_core
        coupecad_logging
        GTest::gtest
        GTest::gtest_main
        Qt6::Core
        Qt6::Test
)

target_include_directories(coupecad_ui_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests/viewport     # reuse FakeRenderer when needed
)

# macOS gtest-include-order workaround (Conan vs Homebrew header clash).
get_target_property(_gtest_inc GTest::gtest INTERFACE_INCLUDE_DIRECTORIES)
if(_gtest_inc)
    target_include_directories(coupecad_ui_test BEFORE PRIVATE ${_gtest_inc})
endif()

include(GoogleTest)
gtest_discover_tests(coupecad_ui_test)
