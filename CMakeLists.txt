cmake_minimum_required(VERSION 3.25)

project(CoupeCAD
    VERSION 0.1.0
    DESCRIPTION "Cross-platform CAD for cabinet furniture"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(COUPECAD_BUILD_TESTS "Build CoupeCAD test suites" ON)

# Renderer backend selector — зарезервировано для Stage 3+ (renderer skeleton).
# В Stage 0 ни одна реализация ещё не подключена; флаг определён, чтобы
# CMake-конфигурация уже понимала это имя.
set(COUPECAD_RENDERER "occt" CACHE STRING "Renderer backend: occt or qq3d")
set_property(CACHE COUPECAD_RENDERER PROPERTY STRINGS occt qq3d)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# В Stage 0 ещё нет ни OpenCASCADE, ни Qt Quick 3D — только Quick + Gui.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.5)

add_subdirectory(src)
add_subdirectory(apps/coupecad)

if(COUPECAD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
