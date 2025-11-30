cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(bicycletub LANGUAGES C CXX)

add_library(bicycletub_lib STATIC
  include/types.h
  include/frame_header.h
  include/disk_manager_memory.h
  include/arc_replacer.h
  include/buffer_pool_manager.h
  include/disk_scheduler.h
  include/page_guard.h
  include/b_plus_tree_page.h
  include/b_plus_tree_key.h
  include/b_plus_tree_internal_page.h
  include/b_plus_tree_leaf_page.h
  include/b_plus_tree_header_page.h
  include/index_iterator.h
  include/b_plus_tree.h
  include/page.h
  include/bnlj.h

  src/disk_manager_memory.cpp
  src/arc_replacer.cpp
  src/buffer_pool_manager.cpp
  src/disk_scheduler.cpp
  src/page_guard.cpp
  src/b_plus_tree_page.cpp
  src/b_plus_tree_internal_page.cpp
  src/b_plus_tree_leaf_page.cpp
  src/index_iterator.cpp
  src/b_plus_tree.cpp
  src/bnlj.cpp
  src/page.cpp
)

target_include_directories(bicycletub_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)


add_executable(bicycletub src/main.cpp)
target_link_libraries(bicycletub bicycletub_lib)

enable_testing()

find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(
  bicycletub_buffer_pool_manager_tests
  tests/buffer_pool_manager_test.cpp
)

add_executable(
  bicycletub_b_plus_tree_tests
  tests/b_plus_tree_single_test.cpp
  tests/b_plus_tree_multi_thread_test.cpp
  tests/b_plus_tree_long_run_test.cpp
)

add_executable(
  bicycletub_bnlj_tests
  tests/bnlj_test.cpp
)
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/bnlj_stress_test.cpp)
  target_sources(bicycletub_bnlj_tests PRIVATE tests/bnlj_stress_test.cpp)
  target_link_libraries(bicycletub_bnlj_tests bicycletub_lib GTest::gtest_main)
  gtest_discover_tests(bicycletub_bnlj_tests)
endif()

target_link_libraries(
  bicycletub_buffer_pool_manager_tests
  bicycletub_lib
  GTest::gtest_main
)

target_link_libraries(
  bicycletub_bnlj_tests
  bicycletub_lib
  GTest::gtest_main
)

target_link_libraries(
  bicycletub_b_plus_tree_tests
  bicycletub_lib
  GTest::gtest_main
)

gtest_discover_tests(bicycletub_buffer_pool_manager_tests)
gtest_discover_tests(bicycletub_b_plus_tree_tests)
gtest_discover_tests(bicycletub_bnlj_tests)
