add_executable(coupecad_viewport_test
    viewport_controller_test.cpp
)

target_link_libraries(coupecad_viewport_test
    PRIVATE
        coupecad_viewport
        coupecad_core
        coupecad_geometry
        coupecad_renderer
        coupecad_logging
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(coupecad_viewport_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}     # so #include "fake_renderer.h" works
)

# coupecad_viewport publicly links Qt6, which transitively adds
# `-isystem /opt/homebrew/include` to the include path. That can shadow
# Conan's gtest headers with a Homebrew gtest of a different ABI, breaking
# the link. Force Conan's gtest include to come first.
get_target_property(_gtest_inc GTest::gtest INTERFACE_INCLUDE_DIRECTORIES)
if(_gtest_inc)
    target_include_directories(coupecad_viewport_test BEFORE PRIVATE ${_gtest_inc})
endif()

include(GoogleTest)
gtest_discover_tests(coupecad_viewport_test)
