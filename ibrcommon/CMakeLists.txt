cmake_minimum_required(VERSION 3.2)

project (ibrcommon)

find_package(PkgConfig REQUIRED QUIET)

option(Debug "Build for debugging [default=OFF]" OFF)

if(Debug)
  message(STATUS "Building for Debugging")
  add_definitions(-D __DEVELOPMENT_ASSERTIONS__)
  # TODO: CXXFLAGS
endif()

include_directories("${CMAKE_SOURCE_DIR}/ibrcommon")

add_subdirectory("ibrcommon")
