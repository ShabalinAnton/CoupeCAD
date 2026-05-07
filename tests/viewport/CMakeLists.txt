add_executable(coupecad_viewport_test
    viewport_controller_test.cpp
    viewport_controller_observer_test.cpp
    viewport_controller_camera_test.cpp
    viewport_controller_pick_test.cpp
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

# QML smoke test — separate target so a failing QML setup doesn't take
# down the controller tests.
find_package(Qt6 6.5 REQUIRED COMPONENTS Qml Quick)

add_executable(coupecad_viewport_qml_smoke_test
    viewport_qml_smoke_test.cpp
)

target_link_libraries(coupecad_viewport_qml_smoke_test
    PRIVATE
        coupecad_viewport
        Qt6::Qml
        Qt6::Quick
        GTest::gtest
        GTest::gtest_main
)

# Apply the same gtest-include-order workaround as the controller-test target.
if(_gtest_inc)
    target_include_directories(coupecad_viewport_qml_smoke_test
        BEFORE PRIVATE ${_gtest_inc})
endif()

set_target_properties(coupecad_viewport_qml_smoke_test PROPERTIES AUTOMOC ON)

gtest_discover_tests(coupecad_viewport_qml_smoke_test
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
