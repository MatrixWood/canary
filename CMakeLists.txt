cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE "Debug")

project(
  canary
  VERSION 0.1.0
  LANGUAGES CXX
)

option(CANARY_OPT_BUILD_UNITTESTS "Build all unittests" ON)
option(CANARY_OPT_BUILD_EXAMPLES "Build all examples" OFF)

message(STATUS "CANARY_OPT_BUILD_UNITTESTS is ${CANARY_OPT_BUILD_UNITTESTS}")
message(STATUS "CANARY_OPT_BUILD_EXAMPLES is ${CANARY_OPT_BUILD_EXAMPLES}")

# CMake helpers:
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

include_directories(base)

add_library(
  ${PROJECT_NAME}
  STATIC
  ${PROJECT_SOURCE_DIR}/base/Date.cc
)

if (CANARY_OPT_BUILD_UNITTESTS)
  add_subdirectory(unittests)
endif()

if (CANARY_OPT_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()