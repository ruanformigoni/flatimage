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

# Tools
## XXD
find_program(XXD_COMMAND xxd)
if(NOT XXD_COMMAND)
  message(FATAL_ERROR "xxd command not found")
endif()
## Git
find_program(GIT_COMMAND git)
if(NOT GIT_COMMAND)
  message(FATAL_ERROR "git command not found")
endif()

# Definitions
## Version
execute_process(
  COMMAND ${GIT_COMMAND} describe --tags --abbrev=0
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_definitions("-DVERSION=\"${VERSION}\"")
message(STATUS "VERSION: ${VERSION}")
## Timestamp
string(TIMESTAMP TIMESTAMP "%Y%m%d%H%M%S")
add_definitions("-DTIMESTAMP=\"${TIMESTAMP}\"")
message(STATUS "TIMESTAMP: ${TIMESTAMP}")
## Commit
execute_process(
  COMMAND ${GIT_COMMAND} rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_definitions("-DCOMMIT=\"${GIT_COMMIT_HASH}\"")
message(STATUS "GIT COMMIT HASH: ${GIT_COMMIT_HASH}")

# External libraries
find_package(nlohmann_json REQUIRED)
find_package(Boost REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)

# Main executable
add_executable(boot boot.cpp)
target_link_libraries(boot PRIVATE
  nlohmann_json::nlohmann_json
  /usr/lib/libturbojpeg.a
  /usr/lib/libpng.a
  /usr/lib/libcom_err.a
  /usr/lib/libz.a
)
target_compile_options(boot PRIVATE -g -rdynamic -static -Wall -Os -Wextra)
target_link_options(boot PRIVATE -static)

# Magic patcher
add_executable(magic magic.cpp)
target_compile_options(magic PRIVATE --std=c++23 -Wall -Wextra)
target_link_options(magic PRIVATE -static)
add_custom_target(run_magic ALL
  DEPENDS boot magic
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/magic ${CMAKE_CURRENT_BINARY_DIR}/boot
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Running magic with boot as argument"
  VERBATIM
)
