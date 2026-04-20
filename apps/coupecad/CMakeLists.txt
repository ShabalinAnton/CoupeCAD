qt_add_executable(coupecad
    WIN32
    MACOSX_BUNDLE
    main.cpp
)

qt_add_qml_module(coupecad
    URI coupecad
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_link_libraries(coupecad
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)

set_target_properties(coupecad PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "app.coupecad.coupecad"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    # On Linux, qt_add_qml_module creates a subdirectory named after the URI
    # ("coupecad/") in CMAKE_CURRENT_BINARY_DIR. The executable would land
    # at the same path, causing a linker error ("Is a directory"). Place the
    # runtime binary one level up to avoid the collision.
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# AGL was removed from macOS 26+ SDK. Supply a minimal TBD stub so the
# linker can resolve the -framework AGL reference injected by Qt6::OpenGL.
# The stub lives in cmake/stubs/AGL.framework/AGL.tbd and is linked
# against the real /System/Library/Frameworks/AGL.framework at runtime.
if(APPLE)
    target_link_options(coupecad PRIVATE
        -F${CMAKE_SOURCE_DIR}/cmake/stubs
    )
endif()
