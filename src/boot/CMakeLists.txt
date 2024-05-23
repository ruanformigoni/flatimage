######################################################################
# @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
# @file        : CMakeLists
######################################################################

cmake_minimum_required(VERSION 3.5)

project(FlatImage DESCRIPTION "FlatImage" LANGUAGES CXX)

# Compiler
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

# Avoid in-source builds
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not allowed")
endif()

# External libraries
find_package(argparse REQUIRED)
find_package(nlohmann_json REQUIRED)

# # Main executable
add_executable(boot boot.cpp)
target_link_libraries(boot PRIVATE
  argparse::argparse 
  nlohmann_json::nlohmann_json
)
target_compile_options(boot PRIVATE -Wall -Os -Wextra)
target_link_options(boot PRIVATE -static)
