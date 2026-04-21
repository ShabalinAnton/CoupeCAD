find_package(stduuid REQUIRED)
find_package(fmt REQUIRED)

add_library(coupecad_core STATIC
    # .cpp-файлы добавляются по мере появления в следующих задачах.
    # Пока target существует только ради публичных заголовков.
    dummy.cpp
)

target_include_directories(coupecad_core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_core
    PUBLIC
        coupecad_logging
        fmt::fmt
    PRIVATE
        stduuid::stduuid
)

target_compile_features(coupecad_core PUBLIC cxx_std_20)
