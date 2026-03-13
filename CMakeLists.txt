cmake_minimum_required(VERSION 3.28)
project(GTFS2RDF LANGUAGES C CXX)

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
    "This project requires the Ninja or Visual Studio generator "
    "to automatically scan and build C++20 modules in the correct order. "
    "Example: cmake -S . -B build -G Ninja"
  )
endif()

# Default build type for single-config generators (Ninja)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Default: don't build tests in Release unless explicitly requested.
# Important: must happen BEFORE include(CTest)/add_subdirectory(tests).
if(NOT CMAKE_CONFIGURATION_TYPES) # single-config only
  if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT DEFINED BUILD_TESTING)
    set(BUILD_TESTING OFF CACHE BOOL "Build tests" FORCE)
  endif()
endif()

# Testing
include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error LANGUAGES C CXX)

# warnings only; don't hardcode -O/-g, let build type/toolchain decide
function(setup_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic)
  endif()
endfunction()

# Custom options
option(GTFS2RDF_FULL_STATS "Enable storage timing even in Release" OFF)
option(GTFS2RDF_ENABLE_IPO "Enable IPO/LTO in Release builds" ON)

# Find libzip (provided by libzip-dev)
find_package(libzip REQUIRED)

# ------------------------------------------------------------
# SQLite (amalgamation)
# ------------------------------------------------------------
add_library(sqlite3_amalgamation STATIC
  "${CMAKE_SOURCE_DIR}/src/third_party/sqlite3/sqlite3.c"
)

target_include_directories(sqlite3_amalgamation PUBLIC
  "${CMAKE_SOURCE_DIR}/src/third_party/sqlite3"
)

# Optional but recommended for embedding
target_compile_definitions(sqlite3_amalgamation PRIVATE
  SQLITE_THREADSAFE=1
  SQLITE_OMIT_LOAD_EXTENSION
)

# ------------------------------------------------------------
# Auto-generate schema primary module + registry from src/schema/*.cppm
# ------------------------------------------------------------
set(SCHEMA_DIR "${CMAKE_SOURCE_DIR}/src/schema")

file(GLOB SCHEMA_PARTITION_FILES CONFIGURE_DEPENDS
  "${SCHEMA_DIR}/*.cppm"
)
list(SORT SCHEMA_PARTITION_FILES)

set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")

set(GENERATED_SCHEMA_CPPM     "${GENERATED_DIR}/schema.cppm")
set(GENERATED_SCHEMA_REG_CPPM "${GENERATED_DIR}/schema_registry.cppm")

# snake_case -> CamelCase helper
function(snake_to_camel out_var in_str)
  string(REPLACE "_" ";" parts "${in_str}")
  set(result "")
  foreach(p IN LISTS parts)
    if(p STREQUAL "")
      continue()
    endif()
    string(SUBSTRING "${p}" 0 1 first)
    string(SUBSTRING "${p}" 1 -1 rest)
    string(TOUPPER "${first}" first_upper)
    string(APPEND result "${first_upper}${rest}")
  endforeach()
  set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

# Collect partition names from file stems
set(SCHEMA_PART_NAMES "")
foreach(f IN LISTS SCHEMA_PARTITION_FILES)
  get_filename_component(stem "${f}" NAME_WE)

  # These are partitions you *don't* want in the auto list
  if(stem STREQUAL "schema")
    continue()
  endif()
  if(stem STREQUAL "registry")
    continue()
  endif()
  if(stem MATCHES ".*_impl$")
    continue()
  endif()

  # Keep core separate (we'll add it first explicitly)
  if(stem STREQUAL "core")
    continue()
  endif()

  list(APPEND SCHEMA_PART_NAMES "${stem}")
endforeach()

# --- Generate schema.cppm (primary interface)
set(_schema_content "export module schema;\n\n")
string(APPEND _schema_content "export import :core;\n")
foreach(name IN LISTS SCHEMA_PART_NAMES)
  string(APPEND _schema_content "export import :${name};\n")
endforeach()
string(APPEND _schema_content "export import :registry;\n")
file(WRITE "${GENERATED_SCHEMA_CPPM}" "${_schema_content}")

# --- Generate schema_registry.cppm (schema:registry)
set(_reg "")
string(APPEND _reg "module;\n")
string(APPEND _reg "#include <unordered_map>\n")
string(APPEND _reg "#include <string>\n\n")

string(APPEND _reg "export module schema:registry;\n\n")

string(APPEND _reg "import runtime;\n")
string(APPEND _reg "import :core;\n")
string(APPEND _reg "import field_transforms;\n")
foreach(name IN LISTS SCHEMA_PART_NAMES)
  string(APPEND _reg "import :${name};\n")
endforeach()
string(APPEND _reg "\n")

string(APPEND _reg "export namespace schema {\n")
string(APPEND _reg "  using Factory = Schema(*)(runtime::RuntimeContainer&);\n")
string(APPEND _reg "  const std::unordered_map<std::string, Factory>& factories();\n")
string(APPEND _reg "}\n\n")

string(APPEND _reg "const std::unordered_map<std::string, schema::Factory>& schema::factories() {\n")
string(APPEND _reg "  static const std::unordered_map<std::string, schema::Factory> f = {\n")

foreach(name IN LISTS SCHEMA_PART_NAMES)
  snake_to_camel(camel "${name}")
  string(APPEND _reg "    {\"${name}.txt\", &schema::build${camel}Schema},\n")
endforeach()

string(APPEND _reg "  };\n")
string(APPEND _reg "  return f;\n")
string(APPEND _reg "}\n")

file(WRITE "${GENERATED_SCHEMA_REG_CPPM}" "${_reg}")

# ------------------------------------------------------------
# collect source files automatically
# ------------------------------------------------------------
file(GLOB_RECURSE GTFS_MODULE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/*.cppm"
)

list(FILTER GTFS_MODULE_SOURCES EXCLUDE REGEX ".*/schema\\.cppm$")
list(FILTER GTFS_MODULE_SOURCES EXCLUDE REGEX ".*/schema_registry\\.cppm$")

file(GLOB_RECURSE GTFS_CPP_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

list(FILTER GTFS_CPP_SOURCES EXCLUDE REGEX ".*/gtfs2rdf\\.cpp$")

add_library(gtfs_modules)

target_sources(gtfs_modules
  PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES
    BASE_DIRS
      "${CMAKE_SOURCE_DIR}/src"
      "${GENERATED_DIR}"
    FILES
      ${GTFS_MODULE_SOURCES}
      "${GENERATED_SCHEMA_CPPM}"
      "${GENERATED_SCHEMA_REG_CPPM}"
)

target_sources(gtfs_modules
  PRIVATE
    ${GTFS_CPP_SOURCES}
)

target_include_directories(gtfs_modules
  PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
)

setup_warnings(gtfs_modules)

# ------------------------------------------------------------
# Executable (main)
# ------------------------------------------------------------
add_executable(gtfs2rdf src/gtfs2rdf.cpp)

target_compile_definitions(gtfs_modules PUBLIC
  $<$<BOOL:${GTFS2RDF_FULL_STATS}>:GTFS2RDF_FULL_STATS=1>
)

# ------------------------------------------------------------
# Performance flags (Release only)
# ------------------------------------------------------------
function(enable_release_optimizations target)
  if(GTFS2RDF_ENABLE_IPO)
    if(ipo_supported)
      set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    else()
      message(STATUS "IPO/LTO disabled for ${target}: ${ipo_error}")
    endif()
  else()
    message(STATUS "IPO/LTO disabled for ${target}: GTFS2RDF_ENABLE_IPO=OFF")
  endif()
endfunction()

enable_release_optimizations(gtfs_modules)
enable_release_optimizations(gtfs2rdf)

target_link_libraries(gtfs_modules
  PUBLIC
    sqlite3_amalgamation
    libzip::zip
)

target_link_libraries(gtfs2rdf PRIVATE gtfs_modules)

target_include_directories(gtfs2rdf
  PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

setup_warnings(gtfs2rdf)