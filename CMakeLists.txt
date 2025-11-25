cmake_minimum_required(VERSION 3.28)

project(GTFS2RDF LANGUAGES CXX)

# ------------------------------------------------------------
# Language / Standard
# ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# C++20 module support: only Ninja and Visual Studio
if(NOT (CMAKE_GENERATOR STREQUAL "Ninja"
        OR CMAKE_GENERATOR MATCHES "Visual Studio"))
    message(FATAL_ERROR
        "Dieses Projekt benötigt den Ninja- oder Visual-Studio-Generator, "
        "damit C++20-Module automatisch gescannt und in richtiger Reihenfolge "
        "gebaut werden können. "
        "Beispiel: cmake -S . -B build -G Ninja"
    )
endif()

# compiler flags
function(setup_warnings_and_opts target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /permissive- /EHsc
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -pedantic
            -O2 -g
        )
    endif()
endfunction()

# ------------------------------------------------------------
# collect source files automatically
# ------------------------------------------------------------

# all module units (*.cppm) anywhere in src/
file(GLOB_RECURSE GTFS_MODULE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cppm"
)

# all standard source files (*.cpp) anywhere in src/
file(GLOB_RECURSE GTFS_CPP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

# main (gtfs2rdf.cpp) will be the executable (hence not part of library)
list(FILTER GTFS_CPP_SOURCES EXCLUDE REGEX ".*/gtfs2rdf.cpp$")

add_library(gtfs_modules)

target_sources(gtfs_modules
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_SOURCE_DIR}/src"
        FILES ${GTFS_MODULE_SOURCES}
)

target_sources(gtfs_modules
    PRIVATE
        ${GTFS_CPP_SOURCES}
)

target_include_directories(gtfs_modules
    PUBLIC
        "${CMAKE_SOURCE_DIR}/src"
)

setup_warnings_and_opts(gtfs_modules)

# ------------------------------------------------------------
# Executable (main)
# ------------------------------------------------------------

add_executable(gtfs2rdf
    src/gtfs2rdf.cpp
)

target_link_libraries(gtfs2rdf
    PRIVATE
        gtfs_modules
)

target_include_directories(gtfs2rdf
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
)

setup_warnings_and_opts(gtfs2rdf)
