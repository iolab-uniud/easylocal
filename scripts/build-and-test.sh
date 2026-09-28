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
  --with-rest        Also build and test the Crow REST adapter.

TOML dependency modes:
  EASYLOCAL_TEST_SYSTEM_TOML=auto|on|off
                     Controls the system-installed toml++ check when TOML
                     testing is enabled. Default: auto.
                       auto: use system toml++ when available, otherwise FetchContent
                       on:   require and use system toml++
                       off:  force FetchContent

TUI dependency modes:
  EASYLOCAL_TEST_SYSTEM_FTXUI=auto|on|off
                     Controls the system-installed FTXUI check when TUI
                     testing is enabled. Default: auto.
                       auto: use system FTXUI when available, otherwise FetchContent
                       on:   require and use system FTXUI
                       off:  force FetchContent

REST dependency modes:
  EASYLOCAL_TEST_SYSTEM_CROW=auto|on|off
                     Controls the system-installed Crow check when REST
                     testing is enabled. Default: auto.
                       auto: use system Crow when available, otherwise FetchContent
                       on:   require and use system Crow
                       off:  force FetchContent

Other options:
  --integration      Also run tests labelled integration.
  -h, --help         Show this help.

Examples:
  ./scripts/build-and-test.sh
  ./scripts/build-and-test.sh release
  ./scripts/build-and-test.sh --with-toml
  ./scripts/build-and-test.sh --with-tui
  ./scripts/build-and-test.sh --with-rest
  ./scripts/build-and-test.sh release --all
  ./scripts/build-and-test.sh --integration
  EASYLOCAL_TEST_SYSTEM_TOML=on ./scripts/build-and-test.sh --with-toml
  EASYLOCAL_TEST_SYSTEM_FTXUI=on ./scripts/build-and-test.sh --with-tui
  EASYLOCAL_TEST_SYSTEM_CROW=on ./scripts/build-and-test.sh --with-rest

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
TEST_REST=off
SYSTEM_TOML_MODE="${EASYLOCAL_TEST_SYSTEM_TOML:-auto}"
SYSTEM_FTXUI_MODE="${EASYLOCAL_TEST_SYSTEM_FTXUI:-auto}"
SYSTEM_CROW_MODE="${EASYLOCAL_TEST_SYSTEM_CROW:-auto}"
RUN_INTEGRATION=off
preset_seen=false
profile_seen=false
explicit_core=false
explicit_toml=false
explicit_tui=false
explicit_rest=false

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
            if [[ "$explicit_toml" == true || "$explicit_tui" == true || "$explicit_rest" == true ]]; then
                die "--core and optional integration flags are mutually exclusive"
            fi
            TEST_TOML=off
            TEST_TUI=off
            TEST_REST=off
            explicit_core=true
            profile_seen=true
            ;;
        --all)
            if [[ "$profile_seen" == true ]]; then
                die "--core and --all are mutually exclusive"
            fi
            TEST_TOML=on
            TEST_TUI=on
            TEST_REST=on
            profile_seen=true
            ;;
        --with-toml)
            if [[ "$explicit_core" == true ]]; then
                die "--core and --with-toml are mutually exclusive"
            fi
            TEST_TOML=on
            explicit_toml=true
            ;;
        --integration)
            RUN_INTEGRATION=on
            ;;
        --with-tui)
            if [[ "$explicit_core" == true ]]; then
                die "--core and --with-tui are mutually exclusive"
            fi
            TEST_TUI=on
            explicit_tui=true
            ;;
        --with-rest)
            if [[ "$explicit_core" == true ]]; then
                die "--core and --with-rest are mutually exclusive"
            fi
            TEST_REST=on
            explicit_rest=true
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

case "$SYSTEM_CROW_MODE" in
    auto|on|off)
        ;;
    *)
        die "EASYLOCAL_TEST_SYSTEM_CROW must be one of: auto, on, off"
        ;;
esac

profile_label="core"
if [[ "$TEST_TOML" == on && "$TEST_TUI" == on && "$TEST_REST" == on ]]; then
    profile_label="all-enabled"
else
    [[ "$TEST_TOML" == on ]] && profile_label="${profile_label}+toml"
    [[ "$TEST_TUI" == on ]] && profile_label="${profile_label}+tui"
    [[ "$TEST_REST" == on ]] && profile_label="${profile_label}+rest"
fi
echo "==> Profile: ${profile_label}"
echo "==> Configure: ${PRESET} (Core only)"
cmake --fresh --preset "$PRESET" \
    -DEASYLOCAL_ENABLE_CONFIG_TOML=OFF \
    -DEASYLOCAL_ENABLE_TUI=OFF \
    -DEASYLOCAL_ENABLE_REST=OFF

echo
echo "==> Build: ${PRESET}"
cmake --build --preset "$PRESET" --parallel

echo
echo "==> Test: ${PRESET}"
if [[ "$RUN_INTEGRATION" == "on" ]]; then
    ctest --preset "$PRESET"
else
    ctest --preset "$PRESET" -LE integration
fi

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
    if [[ "$RUN_INTEGRATION" == "on" ]]; then
        ctest --test-dir "$build_dir" --output-on-failure
    else
        ctest --test-dir "$build_dir" --output-on-failure -LE integration
    fi
}

if [[ "$TEST_TUI" == "on" ]]; then
    system_ftxui_available=false

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
            system_ftxui_available=true
            echo
            echo "==> Build: ${PRESET} + TUI (system FTXUI)"
            cmake --build "$TUI_SYSTEM_BUILD_DIR" --parallel

            echo
            echo "==> Test: ${PRESET} + TUI (system FTXUI)"
            if [[ "$RUN_INTEGRATION" == "on" ]]; then
                ctest --test-dir "$TUI_SYSTEM_BUILD_DIR" --output-on-failure
            else
                ctest --test-dir "$TUI_SYSTEM_BUILD_DIR" --output-on-failure -LE integration
            fi
        elif grep -q "EasyLocal TUI requires FTXUI" "$TUI_SYSTEM_CONFIGURE_LOG"; then
            if [[ "$SYSTEM_FTXUI_MODE" == "on" ]]; then
                die "system FTXUI was required but CMake could not find it"
            fi
            echo "==> Fallback: system FTXUI not found; using FetchContent"
        else
            die "TUI system-dependency configure failed"
        fi
    fi

    if [[ "$SYSTEM_FTXUI_MODE" == "off" || "$system_ftxui_available" == false ]]; then
        run_tui_build_and_test \
            "FetchContent" \
            "build/${PRESET}-tui-fetch" \
            -DEASYLOCAL_FETCH_DEPENDENCIES=ON \
            -DCMAKE_DISABLE_FIND_PACKAGE_ftxui=TRUE
    fi
fi

run_rest_build_and_test() {
    local label="$1"
    local build_dir="$2"
    shift 2

    echo
    echo "==> Configure: ${PRESET} + REST (${label})"
    cmake --fresh --preset "$PRESET" \
        -B "$build_dir" \
        -DEASYLOCAL_ENABLE_REST=ON \
        "$@"

    echo
    echo "==> Build: ${PRESET} + REST (${label})"
    cmake --build "$build_dir" --parallel

    echo
    echo "==> Test: ${PRESET} + REST (${label})"
    if [[ "$RUN_INTEGRATION" == "on" ]]; then
        ctest --test-dir "$build_dir" --output-on-failure
    else
        ctest --test-dir "$build_dir" --output-on-failure -LE integration
    fi
}

if [[ "$TEST_REST" == "on" ]]; then
    system_crow_available=false

    if [[ "$SYSTEM_CROW_MODE" != "off" ]]; then
        REST_SYSTEM_BUILD_DIR="build/${PRESET}-rest-system"
        REST_SYSTEM_CONFIGURE_LOG="${REST_SYSTEM_BUILD_DIR}.configure.log"
        system_crow_cmake_args=()
        system_crow_prefix_path="${CMAKE_PREFIX_PATH:-}"

        # Homebrew formulae, when present, may live outside CMake's default
        # package search prefixes. Crow also needs standalone Asio.
        if command -v brew >/dev/null 2>&1; then
            for formula in crow asio; do
                if brew --prefix "$formula" >/dev/null 2>&1; then
                    formula_prefix="$(brew --prefix "$formula")"
                    if [[ -n "$system_crow_prefix_path" ]]; then
                        system_crow_prefix_path="${system_crow_prefix_path};${formula_prefix}"
                    else
                        system_crow_prefix_path="$formula_prefix"
                    fi
                fi
            done
        fi
        if [[ -n "$system_crow_prefix_path" ]]; then
            system_crow_cmake_args+=("-DCMAKE_PREFIX_PATH=${system_crow_prefix_path}")
        fi

        echo
        echo "==> Probe: ${PRESET} + REST (system Crow)"
        set +e
        cmake --fresh --preset "$PRESET" \
            -B "$REST_SYSTEM_BUILD_DIR" \
            -DEASYLOCAL_ENABLE_REST=ON \
            -DEASYLOCAL_FETCH_DEPENDENCIES=OFF \
            "${system_crow_cmake_args[@]}" \
            >"$REST_SYSTEM_CONFIGURE_LOG" 2>&1
        system_configure_status=$?
        set -e

        cat "$REST_SYSTEM_CONFIGURE_LOG"

        if [[ $system_configure_status -eq 0 ]]; then
            system_crow_available=true
            echo
            echo "==> Build: ${PRESET} + REST (system Crow)"
            cmake --build "$REST_SYSTEM_BUILD_DIR" --parallel

            echo
            echo "==> Test: ${PRESET} + REST (system Crow)"
            if [[ "$RUN_INTEGRATION" == "on" ]]; then
                ctest --test-dir "$REST_SYSTEM_BUILD_DIR" --output-on-failure
            else
                ctest --test-dir "$REST_SYSTEM_BUILD_DIR" --output-on-failure -LE integration
            fi
        elif grep -q "EasyLocal REST requires Crow" "$REST_SYSTEM_CONFIGURE_LOG"; then
            if [[ "$SYSTEM_CROW_MODE" == "on" ]]; then
                die "system Crow was required but CMake could not find it"
            fi
            echo "==> Fallback: system Crow not found; using FetchContent"
        else
            die "REST system-dependency configure failed"
        fi
    fi

    if [[ "$SYSTEM_CROW_MODE" == "off" || "$system_crow_available" == false ]]; then
        run_rest_build_and_test \
            "FetchContent" \
            "build/${PRESET}-rest-fetch" \
            -DEASYLOCAL_FETCH_DEPENDENCIES=ON \
            -DCMAKE_DISABLE_FIND_PACKAGE_Crow=TRUE
    fi
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
    if [[ "$RUN_INTEGRATION" == "on" ]]; then
        ctest --test-dir "$build_dir" --output-on-failure
    else
        ctest --test-dir "$build_dir" --output-on-failure -LE integration
    fi
}

system_toml_available=false

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
        system_toml_available=true
        echo
        echo "==> Build: ${PRESET} + ConfigTOML (system toml++)"
        cmake --build "$TOML_SYSTEM_BUILD_DIR" --parallel

        echo
        echo "==> Test: ${PRESET} + ConfigTOML (system toml++)"
        if [[ "$RUN_INTEGRATION" == "on" ]]; then
            ctest --test-dir "$TOML_SYSTEM_BUILD_DIR" --output-on-failure
        else
            ctest --test-dir "$TOML_SYSTEM_BUILD_DIR" --output-on-failure -LE integration
        fi
    elif grep -q "EasyLocal ConfigTOML requires tomlplusplus" "$TOML_SYSTEM_CONFIGURE_LOG"; then
        if [[ "$SYSTEM_TOML_MODE" == "on" ]]; then
            die "system toml++ was required but CMake could not find it"
        fi
        echo "==> Fallback: system toml++ not found; using FetchContent"
    else
        die "ConfigTOML system-dependency configure failed"
    fi
fi

if [[ "$SYSTEM_TOML_MODE" == "off" || "$system_toml_available" == false ]]; then
    run_toml_build_and_test \
        "FetchContent" \
        "build/${PRESET}-toml-fetch" \
        -DEASYLOCAL_FETCH_DEPENDENCIES=ON \
        -DCMAKE_DISABLE_FIND_PACKAGE_tomlplusplus=TRUE
fi
