#!/usr/bin/env bash

set -euo pipefail

die() {
    echo "ERROR: $*" >&2
    exit 1
}

usage() {
    cat <<'USAGE'
Usage:
  ./scripts/build-and-test.sh [dev|release] [options]

Profiles:
  (default)          Build and test EasyLocal Core only.
  --core             Explicitly select the Core-only profile.
  --all              Build and test Core plus all optional integrations.

Optional integrations:
  --with-toml        Also build and test the ConfigTOML adapter.
  --with-tui         Also build and test the FTXUI tester frontend.

TOML dependency modes:
  EASYLOCAL_TEST_SYSTEM_TOML=auto|on|off
                     Controls the system-installed toml++ check when TOML
                     testing is enabled. Default: auto.
                       auto: test system toml++ when available, otherwise skip
                       on:   require and test system toml++
                       off:  skip the system toml++ check
                     The FetchContent fallback is always tested when TOML
                     testing is enabled.

TUI dependency modes:
  EASYLOCAL_TEST_SYSTEM_FTXUI=auto|on|off
                     Controls the system-installed FTXUI check when TUI
                     testing is enabled. Default: auto.
                       auto: test system FTXUI when available, otherwise skip
                       on:   require and test system FTXUI
                       off:  skip the system FTXUI check
                     The FetchContent fallback is always tested when TUI
                     testing is enabled.

Other options:
  -h, --help         Show this help.

Examples:
  ./scripts/build-and-test.sh
  ./scripts/build-and-test.sh release
  ./scripts/build-and-test.sh --with-toml
  ./scripts/build-and-test.sh --with-tui
  ./scripts/build-and-test.sh release --all
  EASYLOCAL_TEST_SYSTEM_TOML=on ./scripts/build-and-test.sh --with-toml
  EASYLOCAL_TEST_SYSTEM_FTXUI=on ./scripts/build-and-test.sh --with-tui

The default preset is 'dev'.

Set CMAKE_BUILD_PARALLEL_LEVEL to control build parallelism, for example:
  CMAKE_BUILD_PARALLEL_LEVEL=8 ./scripts/build-and-test.sh --all
USAGE
}

cd "$(dirname "$0")/.."

command -v cmake >/dev/null 2>&1 || die "'cmake' not found in PATH"
command -v ctest >/dev/null 2>&1 || die "'ctest' not found in PATH"
[[ -f CMakePresets.json ]] || die "CMakePresets.json not found"

PRESET="dev"
TEST_TOML=off
TEST_TUI=off
SYSTEM_TOML_MODE="${EASYLOCAL_TEST_SYSTEM_TOML:-auto}"
SYSTEM_FTXUI_MODE="${EASYLOCAL_TEST_SYSTEM_FTXUI:-auto}"
preset_seen=false
profile_seen=false
explicit_core=false
explicit_toml=false
explicit_tui=false

for arg in "$@"; do
    case "$arg" in
        dev|release)
            if [[ "$preset_seen" == true ]]; then
                die "multiple CMake presets specified"
            fi
            PRESET="$arg"
            preset_seen=true
            ;;
        --core)
            if [[ "$profile_seen" == true ]]; then
                die "--core and --all are mutually exclusive"
            fi
            if [[ "$explicit_toml" == true || "$explicit_tui" == true ]]; then
                die "--core and optional integration flags are mutually exclusive"
            fi
            TEST_TOML=off
            TEST_TUI=off
            explicit_core=true
            profile_seen=true
            ;;
        --all)
            if [[ "$profile_seen" == true ]]; then
                die "--core and --all are mutually exclusive"
            fi
            TEST_TOML=on
            TEST_TUI=on
            profile_seen=true
            ;;
        --with-toml)
            if [[ "$explicit_core" == true ]]; then
                die "--core and --with-toml are mutually exclusive"
            fi
            TEST_TOML=on
            explicit_toml=true
            ;;
        --with-tui)
            if [[ "$explicit_core" == true ]]; then
                die "--core and --with-tui are mutually exclusive"
            fi
            TEST_TUI=on
            explicit_tui=true
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            die "unknown argument: $arg"
            ;;
    esac
done

case "$SYSTEM_TOML_MODE" in
    auto|on|off)
        ;;
    *)
        die "EASYLOCAL_TEST_SYSTEM_TOML must be one of: auto, on, off"
        ;;
esac

case "$SYSTEM_FTXUI_MODE" in
    auto|on|off)
        ;;
    *)
        die "EASYLOCAL_TEST_SYSTEM_FTXUI must be one of: auto, on, off"
        ;;
esac

profile_label="core"
if [[ "$TEST_TOML" == on && "$TEST_TUI" == on ]]; then
    profile_label="all-enabled"
elif [[ "$TEST_TOML" == on ]]; then
    profile_label="core+toml"
elif [[ "$TEST_TUI" == on ]]; then
    profile_label="core+tui"
fi
echo "==> Profile: ${profile_label}"
echo "==> Configure: ${PRESET}"
cmake --preset "$PRESET"

echo
echo "==> Build: ${PRESET}"
cmake --build --preset "$PRESET" --parallel

echo
echo "==> Test: ${PRESET}"
ctest --preset "$PRESET"

run_tui_build_and_test() {
    local label="$1"
    local build_dir="$2"
    shift 2

    echo
    echo "==> Configure: ${PRESET} + TUI (${label})"
    cmake --fresh --preset "$PRESET" \
        -B "$build_dir" \
        -DEASYLOCAL_ENABLE_TUI=ON \
        "$@"

    echo
    echo "==> Build: ${PRESET} + TUI (${label})"
    cmake --build "$build_dir" --parallel

    echo
    echo "==> Test: ${PRESET} + TUI (${label})"
    ctest --test-dir "$build_dir" --output-on-failure
}

if [[ "$TEST_TUI" == "on" ]]; then
    if [[ "$SYSTEM_FTXUI_MODE" != "off" ]]; then
        TUI_SYSTEM_BUILD_DIR="build/${PRESET}-tui-system"
        TUI_SYSTEM_CONFIGURE_LOG="${TUI_SYSTEM_BUILD_DIR}.configure.log"
        system_ftxui_cmake_args=()
        system_ftxui_prefix_path="${CMAKE_PREFIX_PATH:-}"

        # Homebrew config packages are not guaranteed to be on CMake's
        # default system prefix search path.
        if command -v brew >/dev/null 2>&1 \
                && brew --prefix ftxui >/dev/null 2>&1; then
            brew_ftxui_prefix="$(brew --prefix ftxui)"
            if [[ -n "$system_ftxui_prefix_path" ]]; then
                system_ftxui_prefix_path="${system_ftxui_prefix_path};${brew_ftxui_prefix}"
            else
                system_ftxui_prefix_path="$brew_ftxui_prefix"
            fi
        fi
        if [[ -n "$system_ftxui_prefix_path" ]]; then
            system_ftxui_cmake_args+=("-DCMAKE_PREFIX_PATH=${system_ftxui_prefix_path}")
        fi

        echo
        echo "==> Probe: ${PRESET} + TUI (system FTXUI)"
        set +e
        cmake --fresh --preset "$PRESET" \
            -B "$TUI_SYSTEM_BUILD_DIR" \
            -DEASYLOCAL_ENABLE_TUI=ON \
            -DEASYLOCAL_FETCH_DEPENDENCIES=OFF \
            "${system_ftxui_cmake_args[@]}" \
            >"$TUI_SYSTEM_CONFIGURE_LOG" 2>&1
        system_configure_status=$?
        set -e

        cat "$TUI_SYSTEM_CONFIGURE_LOG"

        if [[ $system_configure_status -eq 0 ]]; then
            echo
            echo "==> Build: ${PRESET} + TUI (system FTXUI)"
            cmake --build "$TUI_SYSTEM_BUILD_DIR" --parallel

            echo
            echo "==> Test: ${PRESET} + TUI (system FTXUI)"
            ctest --test-dir "$TUI_SYSTEM_BUILD_DIR" --output-on-failure
        elif grep -q "EasyLocal TUI requires FTXUI" "$TUI_SYSTEM_CONFIGURE_LOG"; then
            if [[ "$SYSTEM_FTXUI_MODE" == "on" ]]; then
                die "system FTXUI was required but CMake could not find it"
            fi
            echo "==> Skip: system FTXUI not found (EASYLOCAL_TEST_SYSTEM_FTXUI=auto)"
        else
            die "TUI system-dependency configure failed"
        fi
    fi

    run_tui_build_and_test \
        "FetchContent" \
        "build/${PRESET}-tui-fetch" \
        -DEASYLOCAL_FETCH_DEPENDENCIES=ON \
        -DCMAKE_DISABLE_FIND_PACKAGE_ftxui=TRUE
fi

if [[ "$TEST_TOML" != "on" ]]; then
    exit 0
fi

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
    if command -v brew >/dev/null 2>&1 \
            && brew --prefix tomlplusplus >/dev/null 2>&1; then
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
