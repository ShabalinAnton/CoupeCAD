find_package(stduuid REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)

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
    commands/hardware_commands.cpp
    commands/hardware_spec_commands.cpp
    commands/macro_command.cpp
    undo_stack.cpp
    io/json_project_serializer.cpp
)

target_include_directories(coupecad_core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_core
    PUBLIC
        coupecad_logging
        fmt::fmt
        stduuid::stduuid
        nlohmann_json::nlohmann_json
)

target_compile_features(coupecad_core PUBLIC cxx_std_20)
