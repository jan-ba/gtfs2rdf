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
  end-to-end/test_feed.cpp
)

target_link_libraries(e2e_tests PRIVATE
  Catch2::Catch2WithMain
  gtfs_modules
)

catch_discover_tests(e2e_tests
  TEST_PREFIX "e2e."
  PROPERTIES LABELS "e2e"
)

add_custom_command(TARGET e2e_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/mock_data"
          "$<TARGET_FILE_DIR:e2e_tests>/mock_data"
)
