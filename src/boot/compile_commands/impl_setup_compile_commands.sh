#!/bin/bash

set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Find used libraries
readarray -t LIB_NAMES < <(pcregrep -o2 "isystem (.*?/p/(.*?)/p/include)" ./build/Release/compile_commands.json)
readarray -t LIB_PATHS < <(pcregrep -o1 "isystem (.*?/p/(.*?)/p/include)" ./build/Release/compile_commands.json)

# Create libs directory
DIR_LIB="/host/conan-libs"
echo "DIR_LIB: $DIR_LIB"
mkdir -p "$DIR_LIB"

# Copy compile_commands.json
COMPILE_COMMANDS_JSON=/host/compile_commands.json
echo "COMPILE_COMMANDS_JSON: $COMPILE_COMMANDS_JSON"
cp ./build/Release/compile_commands.json "$COMPILE_COMMANDS_JSON"

for (( i=0; i < "${#LIB_NAMES[@]}"; i++ )); do
  # Copy the library to the create directory
  cp -r "${LIB_PATHS[$i]}" "$DIR_LIB/${LIB_NAMES[$i]}"
  # Replace path in compile_commands.json
  sed -i "s|${LIB_PATHS[$i]}|$(dirname "$(dirname "$SCRIPT_DIR")")/conan-libs/${LIB_NAMES[$i]}|" "$COMPILE_COMMANDS_JSON"
done
