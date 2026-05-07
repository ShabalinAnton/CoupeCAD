find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick OpenGL)

add_library(coupecad_viewport STATIC
    viewport_controller.cpp
)

target_include_directories(coupecad_viewport PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(coupecad_viewport
    PUBLIC
        coupecad_core
        coupecad_geometry
        coupecad_renderer
        coupecad_renderer_occt
        coupecad_logging
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::OpenGL
)

target_compile_features(coupecad_viewport PUBLIC cxx_std_20)

set_target_properties(coupecad_viewport PROPERTIES
    AUTOMOC ON
)
