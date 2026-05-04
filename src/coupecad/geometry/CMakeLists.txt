find_package(OpenCASCADE REQUIRED)

add_library(coupecad_geometry STATIC
    occt_helpers.cpp
    panel_shape.cpp
    hardware_shape.cpp
    cabinet_shape.cpp
    geometry_builder.cpp
)

target_include_directories(coupecad_geometry
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

# OCCT в PUBLIC: TopoDS_Shape светится консьюмерам.
# Подключаем минимум, нужный для bbox-операций. Если линковка падает
# на отсутствие символа — добавить недостающий TKxxx-таргет.
target_link_libraries(coupecad_geometry
    PUBLIC
        coupecad_core
        coupecad_logging
        TKernel
        TKMath
        TKG2d
        TKG3d
        TKGeomBase
        TKBRep
        TKTopAlgo
        TKPrim
)

target_compile_features(coupecad_geometry PUBLIC cxx_std_20)
