find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

add_library(coupecad_logging STATIC
    logger.cpp
    log_paths.cpp
)

target_include_directories(coupecad_logging
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_logging
    PUBLIC
        fmt::fmt
    PRIVATE
        spdlog::spdlog
)

target_compile_features(coupecad_logging PUBLIC cxx_std_20)
