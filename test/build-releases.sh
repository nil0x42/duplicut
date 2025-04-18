#!/usr/bin/env bash
# build-releases.sh
# Cross‑compile duplicut for the four target platforms using Zig,
# without modifying the original Makefile (except for STRIP_CMD, already added).

set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

###############################################################################
# Prerequisite checks
###############################################################################
for tool in zig make; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "Error: ${tool} not found in PATH" >&2
        exit 1
    fi
done

# Version tag from Git (falls back to "dev" if no tag)
DUPLICUT_VERSION="$(git describe --tags --always 2>/dev/null || echo dev)"

# Prefer llvm-strip if available (handles Mach-O cleanly on Linux)
STRIP_BIN="$(command -v llvm-strip || command -v strip)"

###############################################################################
# Target matrix (output name => Zig compiler flags)
###############################################################################
declare -A TARGET_FLAGS=(
    [duplicut-linux-x64]="-static -target x86_64-linux-musl"
    [duplicut-linux-arm64]="-static -target aarch64-linux-musl"
    [duplicut-macos-x64]="-target x86_64-macos   -mmacosx-version-min=10.13"
    [duplicut-macos-arm64]="-target aarch64-macos -mmacosx-version-min=11.0"
)

# Minimal, size‑oriented CFLAGS shared by all builds
BASE_CFLAGS="-Iinclude -Wall -Wextra -Wdisabled-optimization -Winline \
-Wdouble-promotion -Wunknown-pragmas -Wno-implicit-fallthrough \
-Wno-error=implicit-function-declaration -ffast-math -O2 -DNDEBUG"

###############################################################################
# Build loop
###############################################################################
for VARIANT in "${!TARGET_FLAGS[@]}"; do
    OUTFILE="$VARIANT-$DUPLICUT_VERSION"
    echo "------------------------------------------------------------"
    echo "Building ${OUTFILE} (${TARGET_FLAGS[$VARIANT]})"
    echo "------------------------------------------------------------"

    # Clean previous objects to avoid architecture mixing
    make distclean

    # Invoke the Makefile in release mode with overridden variables
    make release \
        CC="zig cc" \
        TARGET="${OUTFILE}" \
        FLAGS="${TARGET_FLAGS[$VARIANT]} -s" \
        CFLAGS="${BASE_CFLAGS}" \
        STRIP_CMD="${STRIP_BIN}"
done

echo
echo "All binaries built successfully:"
ls -lh duplicut-*

exit 0

