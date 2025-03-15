cmake_minimum_required(VERSION 3.20)
project(
  logloom
  VERSION 0.1.0
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
set(CPM_DOWNLOAD_VERSION "0.40.8")
set(CPM_HASH_SUM "78ba32abdf798bc616bab7c73aac32a17bbd7b06ad9e26a6add69de8f3ae4791")
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
CPMAddPackage("gh:google/googletest@1.16.0")

### Targets
enable_testing()

function(loom_add_test name)
  add_executable(${name} ${name}.cc)
  target_link_libraries(${name} PRIVATE GTest::gtest_main)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_subdirectory(src)
