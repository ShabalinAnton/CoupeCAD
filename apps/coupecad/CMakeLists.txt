qt_add_executable(coupecad
    WIN32
    MACOSX_BUNDLE
    main.cpp
    demo_project.cpp
)

qt_add_qml_module(coupecad
    URI coupecad
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/CabinetPropertiesPanel.qml
)

target_link_libraries(coupecad
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        coupecad_viewport
        coupecad_ui
)

set_target_properties(coupecad PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "app.coupecad.coupecad"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(APPLE)
    target_link_options(coupecad PRIVATE
        -F${CMAKE_SOURCE_DIR}/cmake/stubs
    )
endif()
