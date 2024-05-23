#!/bin/bash

set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Find used libraries
readarray -t LIB_NAMES < <(pcregrep -o2 "isystem (.*?/p/(.*?)/p/include)" ./build/Release/compile_commands.json)
readarray -t LIB_PATHS < <(pcregrep -o1 "isystem (.*?/p/(.*?)/p/include)" ./build/Release/compile_commands.json)

# Create libs directory
DIR_LIB="$(dirname "$SCRIPT_DIR")/conan-libs"
mkdir -p "$DIR_LIB"

# Copy compile_commands.json
COMPILE_COMMANDS_JSON="$(dirname "$SCRIPT_DIR")"/compile_commands.json
cp ./build/Release/compile_commands.json "$COMPILE_COMMANDS_JSON"

for (( i=0; i < "${#LIB_NAMES[@]}"; i++ )); do
  # Copy the library to the create directory
  cp -r "${LIB_PATHS[$i]}" "$DIR_LIB/${LIB_NAMES[$i]}"
  # Replace path in compile_commands.json
  sed -i "s|${LIB_PATHS[$i]}|$DIR_LIB/${LIB_NAMES[$i]}|" "$COMPILE_COMMANDS_JSON"
done
