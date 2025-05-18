cmake_minimum_required(VERSION 3.24)
project(
  oned
  VERSION 0.0.1
  LANGUAGES CXX C
)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/cmake)

### Options
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

### CPM
set(CPM_DOWNLOAD_VERSION "0.42.0")
set(CPM_HASH_SUM "2020b4fc42dba44817983e06342e682ecfc3d2f484a581f11cc5731fbe4dce8a")
set(CPM_DOWNLOAD_URL "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake")

if(CPM_SOURCE_CACHE)
  set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
  set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
  set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

get_filename_component(CPM_DOWNLOAD_LOCATION ${CPM_DOWNLOAD_LOCATION} ABSOLUTE)

if (NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
  file(DOWNLOAD ${CPM_DOWNLOAD_URL} ${CPM_DOWNLOAD_LOCATION}
       EXPECTED_HASH SHA256=${CPM_HASH_SUM})
endif()

include(${CPM_DOWNLOAD_LOCATION})

### Library
CPMAddPackage(
  URI     "gh:abseil/abseil-cpp#20250512.0"
  OPTIONS "ABSL_ENABLE_INSTALL ON")
CPMAddPackage("gh:fmtlib/fmt#11.2.0")
CPMAddPackage("gh:zhscn/outcome@0.1.0")
CPMAddPackage("gh:google/googletest@1.17.0")
find_package(Boost REQUIRED)

### Targets
add_library(oned-core src/chunk.cc src/chunk_manager.cc)
target_link_libraries(
  oned-core
  PUBLIC
  absl::log
  fmt::fmt
  outcome::outcome
  Boost::boost
)
target_compile_options(oned-core PUBLIC -Wall -Wextra)

add_executable(oned src/main.cc)
target_link_libraries(
  oned
  PRIVATE
  absl::log_initialize
  absl::flags
  absl::flags_parse
  # requires CMake 3.24
  $<LINK_LIBRARY:WHOLE_ARCHIVE,absl::log_flags>
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
