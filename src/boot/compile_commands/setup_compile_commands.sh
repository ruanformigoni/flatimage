#!/bin/bash

set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# goto parent dir
cd "$(dirname "$SCRIPT_DIR")"

docker run -it --rm -v"$(pwd)":"$(pwd)" flatimage-boot "$(pwd)/compile_commands/impl_setup_compile_commands.sh"
