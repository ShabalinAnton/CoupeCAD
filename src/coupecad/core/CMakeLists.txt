find_package(stduuid REQUIRED)
find_package(fmt REQUIRED)

add_library(coupecad_core STATIC
    id.cpp
    material.cpp
    hardware.cpp
    panel.cpp
    cabinet.cpp
    project.cpp
    geometry.cpp
    commands/change_set.cpp
    commands/cabinet_commands.cpp
    commands/panel_commands.cpp
    commands/material_commands.cpp
)

target_include_directories(coupecad_core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_core
    PUBLIC
        coupecad_logging
        fmt::fmt
        stduuid::stduuid
)

target_compile_features(coupecad_core PUBLIC cxx_std_20)
