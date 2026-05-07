add_library(coupecad_renderer STATIC
    entity_id.cpp
)
target_include_directories(coupecad_renderer PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(coupecad_renderer PUBLIC coupecad_core)
target_compile_features(coupecad_renderer PUBLIC cxx_std_20)

# Селектор бэкенда. По умолчанию "occt"; объявлен в корневом CMakeLists.
if(COUPECAD_RENDERER STREQUAL "occt")
    add_subdirectory(occt)
elseif(COUPECAD_RENDERER STREQUAL "qq3d")
    message(FATAL_ERROR
        "COUPECAD_RENDERER=qq3d backend lands in Stage 11; "
        "use 'occt' for now.")
else()
    message(FATAL_ERROR "Unknown COUPECAD_RENDERER='${COUPECAD_RENDERER}' "
        "(expected 'occt' or 'qq3d').")
endif()
