######################################################################
# @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
# @file        : CMakeLists
# @created     : Thursday Jan 26, 2023 12:17:13 -03
######################################################################

cmake_minimum_required(VERSION 3.1)

project(Arts DESCRIPTION "Application Chroot Subsystem" LANGUAGES CXX)

# Conan
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})

# Compiler
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")
add_definitions("--std=c++17 -Wall -Wextra")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
message(STATUS "CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
set(CMAKE_BUILD_TYPE Debug)

# External libraries
if(NOT fmt_FOUND)
  find_package(fmt REQUIRED)
endif()

if(NOT range-v3_FOUND)
  find_package(range-v3 REQUIRED)
endif()

add_executable(main main.cpp)
target_link_libraries(main PRIVATE fmt::fmt)
target_link_libraries(main PRIVATE range-v3::range-v3)
