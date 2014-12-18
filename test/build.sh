#!/bin/bash

# Build a release version for use
# with existing tests.

# Usage:
#   ./build.sh
#   ./build.sh <ARCH>
#
# Example:
#   ./build.sh "x64"

set -ve

CC="$CC"
FLAGS="$FLAGS"

make fclean
make
make fclean
make release
