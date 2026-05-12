find_package(Qt6 6.5 REQUIRED COMPONENTS Core)

add_library(coupecad_ui STATIC
    undo_stack_proxy.cpp
)

target_include_directories(coupecad_ui PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(coupecad_ui
    PUBLIC
        coupecad_core
        coupecad_viewport
        coupecad_logging
        Qt6::Core
)

target_compile_features(coupecad_ui PUBLIC cxx_std_20)

set_target_properties(coupecad_ui PROPERTIES AUTOMOC ON)
