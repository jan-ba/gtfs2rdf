cmake_minimum_required(VERSION 3.28)

include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.6.0
)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

# unit tests
add_executable(unit_tests

  unit/test_rdf_escaping.cpp
  unit/test_schema_parser.cpp
  unit/test_storage.cpp
  unit/test_transforms.cpp
  unit/test_util_strings.cpp
  unit/test_util_topological_sort.cpp
)

target_link_libraries(unit_tests PRIVATE
  Catch2::Catch2WithMain
  gtfs_modules
)

catch_discover_tests(unit_tests
  TEST_PREFIX "unit."
  PROPERTIES LABELS "unit"
)

# end-to-end tests
add_executable(e2e_tests
  end-to-end/e2e.cpp
)

target_sources(e2e_tests
  PRIVATE
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
      # tiny feed test schemas
      end-to-end/test_feed_tiny/test_tiny_feed_info.cppm
      end-to-end/test_feed_tiny/test_tiny_stops.cppm
      # full feed test schemas
      end-to-end/test_feed_full/test_full_agency.cppm
      end-to-end/test_feed_full/test_full_calendar_dates.cppm
      end-to-end/test_feed_full/test_full_calendar.cppm
      end-to-end/test_feed_full/test_full_feed_info.cppm
      end-to-end/test_feed_full/test_full_routes.cppm
      end-to-end/test_feed_full/test_full_shapes.cppm
      end-to-end/test_feed_full/test_full_stop_times.cppm
      end-to-end/test_feed_full/test_full_stops.cppm
      end-to-end/test_feed_full/test_full_trips.cppm
)

target_link_libraries(e2e_tests PRIVATE
  Catch2::Catch2WithMain
  gtfs_modules
)

target_compile_definitions(e2e_tests PRIVATE
  GTFS2RDF_E2E_DIR="${CMAKE_CURRENT_SOURCE_DIR}/end-to-end"
)

catch_discover_tests(e2e_tests
  TEST_PREFIX "e2e."
  PROPERTIES LABELS "e2e"
)

