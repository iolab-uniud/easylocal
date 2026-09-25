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
It also verifies the optional ConfigTOML adapter in two dependency modes:
  1. system-installed toml++ when available;
  2. forced FetchContent fallback.

The system-installed TOML check defaults to 'auto': it runs when toml++ can be
found and is otherwise skipped. Set EASYLOCAL_TEST_SYSTEM_TOML=on to require it,
or EASYLOCAL_TEST_SYSTEM_TOML=off to skip it explicitly. CI uses 'on'.

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
SYSTEM_TOML_MODE="${EASYLOCAL_TEST_SYSTEM_TOML:-auto}"

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

case "$SYSTEM_TOML_MODE" in
    auto|on|off)
        ;;
    *)
        die "EASYLOCAL_TEST_SYSTEM_TOML must be one of: auto, on, off"
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

run_toml_build_and_test() {
    local label="$1"
    local build_dir="$2"
    shift 2

    echo
    echo "==> Configure: ${PRESET} + ConfigTOML (${label})"
    cmake --fresh --preset "$PRESET" \
        -B "$build_dir" \
        -DEASYLOCAL_ENABLE_CONFIG_TOML=ON \
        "$@"

    echo
    echo "==> Build: ${PRESET} + ConfigTOML (${label})"
    cmake --build "$build_dir" --parallel

    echo
    echo "==> Test: ${PRESET} + ConfigTOML (${label})"
    ctest --test-dir "$build_dir" --output-on-failure
}

if [[ "$SYSTEM_TOML_MODE" != "off" ]]; then
    TOML_SYSTEM_BUILD_DIR="build/${PRESET}-toml-system"
    TOML_SYSTEM_CONFIGURE_LOG="${TOML_SYSTEM_BUILD_DIR}.configure.log"
    system_toml_cmake_args=()
    system_toml_prefix_path="${CMAKE_PREFIX_PATH:-}"

    # Homebrew installs config packages below its own prefix, which is not
    # guaranteed to be part of CMake's default system prefix search path.
    if command -v brew >/dev/null 2>&1             && brew --prefix tomlplusplus >/dev/null 2>&1; then
        brew_toml_prefix="$(brew --prefix tomlplusplus)"
        if [[ -n "$system_toml_prefix_path" ]]; then
            system_toml_prefix_path="${system_toml_prefix_path};${brew_toml_prefix}"
        else
            system_toml_prefix_path="$brew_toml_prefix"
        fi
    fi
    if [[ -n "$system_toml_prefix_path" ]]; then
        system_toml_cmake_args+=("-DCMAKE_PREFIX_PATH=${system_toml_prefix_path}")
    fi

    echo
    echo "==> Probe: ${PRESET} + ConfigTOML (system toml++)"
    set +e
    cmake --fresh --preset "$PRESET" \
        -B "$TOML_SYSTEM_BUILD_DIR" \
        -DEASYLOCAL_ENABLE_CONFIG_TOML=ON \
        -DEASYLOCAL_FETCH_DEPENDENCIES=OFF \
        "${system_toml_cmake_args[@]}" \
        >"$TOML_SYSTEM_CONFIGURE_LOG" 2>&1
    system_configure_status=$?
    set -e

    cat "$TOML_SYSTEM_CONFIGURE_LOG"

    if [[ $system_configure_status -eq 0 ]]; then
        echo
        echo "==> Build: ${PRESET} + ConfigTOML (system toml++)"
        cmake --build "$TOML_SYSTEM_BUILD_DIR" --parallel

        echo
        echo "==> Test: ${PRESET} + ConfigTOML (system toml++)"
        ctest --test-dir "$TOML_SYSTEM_BUILD_DIR" --output-on-failure
    elif grep -q "EasyLocal ConfigTOML requires tomlplusplus" "$TOML_SYSTEM_CONFIGURE_LOG"; then
        if [[ "$SYSTEM_TOML_MODE" == "on" ]]; then
            die "system toml++ was required but CMake could not find it"
        fi
        echo "==> Skip: system toml++ not found (EASYLOCAL_TEST_SYSTEM_TOML=auto)"
    else
        die "ConfigTOML system-dependency configure failed"
    fi
fi

run_toml_build_and_test \
    "FetchContent" \
    "build/${PRESET}-toml-fetch" \
    -DEASYLOCAL_FETCH_DEPENDENCIES=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_tomlplusplus=TRUE
