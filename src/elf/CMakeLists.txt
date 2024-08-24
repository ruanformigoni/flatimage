######################################################################
# @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
# @file        : CMakeLists
# @created     : Thursday Jan 26, 2023 12:17:13 -03
######################################################################

cmake_minimum_required(VERSION 3.1)

project(Fim DESCRIPTION "Application Chroot Subsystem" LANGUAGES CXX)

# Compiler
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

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

# # Define the command to generate boot.h from _boot.sh
# add_custom_command(
#   OUTPUT boot.h
#   COMMAND ${XXD_COMMAND} -i "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/_boot.sh" > "${CMAKE_CURRENT_SOURCE_DIR}/boot.h"
#   COMMAND sed -Ei "s|unsigned char.*=|unsigned char _script_boot\[\] =|g" "${CMAKE_CURRENT_SOURCE_DIR}/boot.h"
#   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/_boot.sh"
#   COMMENT "Generating boot.h from _boot.sh"
#   VERBATIM
# )

# Define the command to generate killer.h from _killer.sh
add_custom_command(
  OUTPUT killer.h
  COMMAND ${XXD_COMMAND} -i "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/_killer.sh" > "${CMAKE_CURRENT_SOURCE_DIR}/killer.h"
  COMMAND sed -Ei "s|unsigned char.*=|unsigned char _script_killer\[\] =|g" "${CMAKE_CURRENT_SOURCE_DIR}/killer.h"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/_killer.sh"
  COMMENT "Generating killer.h from _killer.sh"
  VERBATIM
)

# Definitions
## Timestamp
string(TIMESTAMP TIMESTAMP "%Y%m%d%H%M%S")
add_definitions("-DTIMESTAMP=${TIMESTAMP}")
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

# Main executable
add_executable(main main.cpp killer.h)
target_compile_options(main PRIVATE --std=c++23 -Wall -Wextra)
target_link_options(main PRIVATE -static)

# Magic patcher
add_executable(magic magic.cpp)
target_compile_options(magic PRIVATE --std=c++23 -Wall -Wextra)
target_link_options(magic PRIVATE -static)
