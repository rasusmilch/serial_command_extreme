cmake_minimum_required(VERSION 3.20)

project(
  serial_command_extreme
  VERSION 0.1.0
  DESCRIPTION "Bounded embedded serial command parser/dispatcher library"
  LANGUAGES C
)

# This is an early scaffold. The repository is currently anchor/planning only.
# Keep this file host-oriented; firmware/toolchain-specific builds belong in
# adapters or integration-specific build files after the core exists.

option(SCE_BUILD_TESTS "Build host tests when test/CMakeLists.txt exists" ON)
option(SCE_BUILD_EXAMPLES "Build host examples when examples/CMakeLists.txt exists" ON)

add_library(serial_command_extreme INTERFACE)
add_library(serial_command_extreme::serial_command_extreme ALIAS serial_command_extreme)

target_include_directories(
  serial_command_extreme
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(serial_command_extreme INTERFACE c_std_99)

if(SCE_BUILD_TESTS)
  enable_testing()
  if(EXISTS "${PROJECT_SOURCE_DIR}/test/CMakeLists.txt")
    add_subdirectory(test)
  else()
    message(STATUS "SCE_BUILD_TESTS is ON, but test/CMakeLists.txt does not exist yet.")
  endif()
endif()

if(SCE_BUILD_EXAMPLES)
  if(EXISTS "${PROJECT_SOURCE_DIR}/examples/CMakeLists.txt")
    add_subdirectory(examples)
  else()
    message(STATUS "SCE_BUILD_EXAMPLES is ON, but examples/CMakeLists.txt does not exist yet.")
  endif()
endif()
