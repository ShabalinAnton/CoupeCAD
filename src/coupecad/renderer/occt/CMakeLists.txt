find_package(OpenCASCADE REQUIRED)

add_library(coupecad_renderer_occt STATIC
    view_driver.cpp
)

target_link_libraries(coupecad_renderer_occt
    PUBLIC
        coupecad_renderer
        coupecad_geometry
        coupecad_logging
    PRIVATE
        # Visualization stack (поверх Stage 2 OCCT-таргетов).
        TKV3d
        TKOpenGl
        TKService
        TKHLR
        TKMeshVS
)

target_compile_features(coupecad_renderer_occt PUBLIC cxx_std_20)
