#!/bin/bash

set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# goto parent dir
cd "$(dirname "$(dirname "$SCRIPT_DIR")")"

docker run -it --rm -v"$(pwd)":/host flatimage-boot ./compile_commands/impl_setup_compile_commands.sh
