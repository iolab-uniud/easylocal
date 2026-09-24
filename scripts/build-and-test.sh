#!/usr/bin/env bash

set -euo pipefail

die() {
    echo "ERROR: $*" >&2
    exit 1
}

usage() {
    cat <<'USAGE'
Usage:
  ./scripts/build-and-test.sh
  ./scripts/build-and-test.sh dev
  ./scripts/build-and-test.sh release

Configures, builds, and runs the test suite using the matching CMake preset.
The default preset is 'dev'.

Set CMAKE_BUILD_PARALLEL_LEVEL to control build parallelism, for example:
  CMAKE_BUILD_PARALLEL_LEVEL=8 ./scripts/build-and-test.sh
USAGE
}

cd "$(dirname "$0")/.."

command -v cmake >/dev/null 2>&1 || die "'cmake' not found in PATH"
command -v ctest >/dev/null 2>&1 || die "'ctest' not found in PATH"
[[ -f CMakePresets.json ]] || die "CMakePresets.json not found"

PRESET="${1:-dev}"

case "$PRESET" in
    dev|release)
        ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        usage >&2
        exit 1
        ;;
esac

echo "==> Configure: ${PRESET}"
cmake --preset "$PRESET"

echo
echo "==> Build: ${PRESET}"
cmake --build --preset "$PRESET" --parallel

echo
echo "==> Test: ${PRESET}"
ctest --preset "$PRESET"
