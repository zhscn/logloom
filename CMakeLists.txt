cmake_minimum_required(VERSION 3.24)
project(
  oned
  VERSION 0.0.1
  LANGUAGES CXX C
)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

### Options
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

### Library
find_package(Boost REQUIRED)
find_package(fmt REQUIRED)
find_package(GTest REQUIRED)
add_subdirectory(src/outcome)

### Targets
add_library(oned-core src/chunk.cc src/chunk_manager.cc)
target_link_libraries(
  oned-core
  PUBLIC
  fmt::fmt
  outcome::outcome
  Boost::boost
)
target_compile_options(oned-core PUBLIC -Wall -Wextra)

add_executable(oned src/main.cc)
target_link_libraries(
  oned
  PRIVATE
  oned-core
)

set_property(
  SOURCE src/main.cc
  PROPERTY COMPILE_DEFINITIONS
  ONED_VERSION="${PROJECT_VERSION}"
  ONED_ROOT="${PROJECT_SOURCE_DIR}")

enable_testing()

function(oned_add_test name)
  add_executable(${name} ${name}.cc)
  target_link_libraries(${name} PRIVATE oned-core GTest::gtest_main)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_subdirectory(src)
