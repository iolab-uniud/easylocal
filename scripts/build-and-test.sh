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

Execution modes:
  (default)          Configure/build/test the requested combination once.
                     For example, --with-tui --with-rest produces one build
                     containing Core + TUI + REST.
  --exhaustive       Test every feature subset of the requested combination.
                     For TUI + REST this checks Core, TUI, REST, and TUI+REST.
                     With --all this checks the complete 8-profile matrix.

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
  --integration      Also run all tests labelled integration. REST HTTP/curl
                     integration is always exercised by --with-rest; this flag
                     additionally enables the other integration tests.
  -h, --help         Show this help.

Examples:
  ./scripts/build-and-test.sh
  ./scripts/build-and-test.sh release
  ./scripts/build-and-test.sh --with-toml
  ./scripts/build-and-test.sh --with-tui --with-rest
  ./scripts/build-and-test.sh --all
  ./scripts/build-and-test.sh --all --exhaustive
  ./scripts/build-and-test.sh --with-tui --with-rest --exhaustive
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

# An explicit level for ctest -j: without a value it needs CMake 3.29.
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"

PRESET="dev"
TEST_TOML=off
TEST_TUI=off
TEST_REST=off
SYSTEM_TOML_MODE="${EASYLOCAL_TEST_SYSTEM_TOML:-auto}"
SYSTEM_FTXUI_MODE="${EASYLOCAL_TEST_SYSTEM_FTXUI:-auto}"
SYSTEM_CROW_MODE="${EASYLOCAL_TEST_SYSTEM_CROW:-auto}"
RUN_INTEGRATION=off
EXHAUSTIVE=off
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
        --integration)
            RUN_INTEGRATION=on
            ;;
        --exhaustive)
            EXHAUSTIVE=on
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

validate_dependency_mode() {
    local variable="$1"
    local value="$2"
    case "$value" in
        auto|on|off)
            ;;
        *)
            die "$variable must be one of: auto, on, off"
            ;;
    esac
}

validate_dependency_mode EASYLOCAL_TEST_SYSTEM_TOML "$SYSTEM_TOML_MODE"
validate_dependency_mode EASYLOCAL_TEST_SYSTEM_FTXUI "$SYSTEM_FTXUI_MODE"
validate_dependency_mode EASYLOCAL_TEST_SYSTEM_CROW "$SYSTEM_CROW_MODE"

system_prefix_path="${CMAKE_PREFIX_PATH:-}"
append_system_prefix() {
    local prefix="$1"
    [[ -n "$prefix" ]] || return
    if [[ -n "$system_prefix_path" ]]; then
        system_prefix_path="${system_prefix_path};${prefix}"
    else
        system_prefix_path="$prefix"
    fi
}

# Homebrew config packages are not guaranteed to be on CMake's default
# system prefix search path. Add known optional dependency prefixes once so
# combined profiles can discover all of them in a single configure.
if command -v brew >/dev/null 2>&1; then
    for formula in tomlplusplus ftxui crow asio; do
        if brew --prefix "$formula" >/dev/null 2>&1; then
            append_system_prefix "$(brew --prefix "$formula")"
        fi
    done
fi

system_prefix_args=()
if [[ -n "$system_prefix_path" ]]; then
    system_prefix_args+=("-DCMAKE_PREFIX_PATH=${system_prefix_path}")
fi

RESOLVED_TOML=off
RESOLVED_TUI=off
RESOLVED_REST=off

resolve_dependency() {
    local mode="$1"
    local label="$2"
    local enable_variable="$3"
    local missing_marker="$4"
    local output_variable="$5"
    local slug="$6"

    if [[ "$mode" == off ]]; then
        printf -v "$output_variable" '%s' fetch
        return
    fi

    local probe_dir="build/.easylocal-probes/${PRESET}-${slug}-system"
    local probe_log="${probe_dir}.configure.log"
    mkdir -p "$(dirname "$probe_dir")"

    echo
    echo "==> Probe: ${label} (system dependency)"
    set +e
    cmake --fresh --preset "$PRESET" \
        -B "$probe_dir" \
        -DEASYLOCAL_BUILD_TESTS=OFF \
        -DEASYLOCAL_BUILD_EXAMPLES=OFF \
        -DEASYLOCAL_ENABLE_CONFIG_TOML=OFF \
        -DEASYLOCAL_ENABLE_TUI=OFF \
        -DEASYLOCAL_ENABLE_REST=OFF \
        "-D${enable_variable}=ON" \
        -DEASYLOCAL_FETCH_DEPENDENCIES=OFF \
        ${system_prefix_args[@]+"${system_prefix_args[@]}"} \
        >"$probe_log" 2>&1
    local probe_status=$?
    set -e

    if [[ $probe_status -eq 0 ]]; then
        printf -v "$output_variable" '%s' system
        echo "==> Found: ${label} system dependency"
        return
    fi

    if grep -q "$missing_marker" "$probe_log"; then
        if [[ "$mode" == on ]]; then
            cat "$probe_log"
            die "system dependency required for ${label}, but CMake could not find it"
        fi
        printf -v "$output_variable" '%s' fetch
        echo "==> Fallback: ${label} system dependency not found; using FetchContent"
        return
    fi

    cat "$probe_log"
    die "${label} system-dependency probe failed"
}

if [[ "$TEST_TOML" == on ]]; then
    resolve_dependency \
        "$SYSTEM_TOML_MODE" \
        "ConfigTOML / toml++" \
        EASYLOCAL_ENABLE_CONFIG_TOML \
        "EasyLocal ConfigTOML requires tomlplusplus" \
        RESOLVED_TOML \
        toml
fi

if [[ "$TEST_TUI" == on ]]; then
    resolve_dependency \
        "$SYSTEM_FTXUI_MODE" \
        "TUI / FTXUI" \
        EASYLOCAL_ENABLE_TUI \
        "EasyLocal TUI requires FTXUI" \
        RESOLVED_TUI \
        tui
fi

if [[ "$TEST_REST" == on ]]; then
    resolve_dependency \
        "$SYSTEM_CROW_MODE" \
        "REST / Crow" \
        EASYLOCAL_ENABLE_REST \
        "EasyLocal REST requires Crow" \
        RESOLVED_REST \
        rest
fi

profile_name() {
    local toml="$1"
    local tui="$2"
    local rest="$3"
    local parts=()
    [[ "$toml" == on ]] && parts+=(toml)
    [[ "$tui" == on ]] && parts+=(tui)
    [[ "$rest" == on ]] && parts+=(rest)
    if [[ ${#parts[@]} -eq 0 ]]; then
        printf '%s' core
        return
    fi
    local IFS=-
    printf '%s' "${parts[*]}"
}

dependency_profile_name() {
    local toml="$1"
    local tui="$2"
    local rest="$3"
    local modes=()
    [[ "$toml" == on ]] && modes+=("$RESOLVED_TOML")
    [[ "$tui" == on ]] && modes+=("$RESOLVED_TUI")
    [[ "$rest" == on ]] && modes+=("$RESOLVED_REST")

    if [[ ${#modes[@]} -eq 0 ]]; then
        printf '%s' none
        return
    fi

    local first="${modes[0]}"
    local mode
    for mode in "${modes[@]}"; do
        if [[ "$mode" != "$first" ]]; then
            printf '%s' mixed
            return
        fi
    done
    printf '%s' "$first"
}

run_profile() {
    local toml="$1"
    local tui="$2"
    local rest="$3"
    local profile
    local dependency_profile
    profile="$(profile_name "$toml" "$tui" "$rest")"
    dependency_profile="$(dependency_profile_name "$toml" "$tui" "$rest")"

    local build_dir
    if [[ "$profile" == core ]]; then
        build_dir="build/${PRESET}"
    else
        build_dir="build/${PRESET}-${profile}-${dependency_profile}"
    fi

    local enable_toml=OFF
    local enable_tui=OFF
    local enable_rest=OFF
    [[ "$toml" == on ]] && enable_toml=ON
    [[ "$tui" == on ]] && enable_tui=ON
    [[ "$rest" == on ]] && enable_rest=ON

    local fetch_dependencies=OFF
    local dependency_args=()
    if [[ "$toml" == on && "$RESOLVED_TOML" == fetch ]]; then
        fetch_dependencies=ON
        dependency_args+=("-DCMAKE_DISABLE_FIND_PACKAGE_tomlplusplus=TRUE")
    fi
    if [[ "$tui" == on && "$RESOLVED_TUI" == fetch ]]; then
        fetch_dependencies=ON
        dependency_args+=("-DCMAKE_DISABLE_FIND_PACKAGE_ftxui=TRUE")
    fi
    if [[ "$rest" == on && "$RESOLVED_REST" == fetch ]]; then
        fetch_dependencies=ON
        dependency_args+=("-DCMAKE_DISABLE_FIND_PACKAGE_Crow=TRUE")
    fi

    echo
    echo "==> Profile: ${profile} (${dependency_profile})"
    echo "==> Configure: ${build_dir}"
    cmake --fresh --preset "$PRESET" \
        -B "$build_dir" \
        "-DEASYLOCAL_ENABLE_CONFIG_TOML=${enable_toml}" \
        "-DEASYLOCAL_ENABLE_TUI=${enable_tui}" \
        "-DEASYLOCAL_ENABLE_REST=${enable_rest}" \
        "-DEASYLOCAL_FETCH_DEPENDENCIES=${fetch_dependencies}" \
        ${system_prefix_args[@]+"${system_prefix_args[@]}"} \
        ${dependency_args[@]+"${dependency_args[@]}"}

    echo
    echo "==> Build: ${build_dir}"
    cmake --build "$build_dir" --parallel

    echo
    echo "==> Test: ${build_dir}"
    if [[ "$RUN_INTEGRATION" == on ]]; then
        ctest --test-dir "$build_dir" --output-on-failure -j "$JOBS"
    else
        ctest --test-dir "$build_dir" --output-on-failure -j "$JOBS" -LE integration
        if [[ "$rest" == on ]]; then
            echo
            echo "==> REST HTTP integration: ${build_dir}"
            ctest --test-dir "$build_dir" \
                --output-on-failure -j "$JOBS" \
                --no-tests=ignore \
                -L rest-http
        fi
    fi
}

requested_mask=0
[[ "$TEST_TOML" == on ]] && requested_mask=$((requested_mask | 1))
[[ "$TEST_TUI" == on ]] && requested_mask=$((requested_mask | 2))
[[ "$TEST_REST" == on ]] && requested_mask=$((requested_mask | 4))

if [[ "$EXHAUSTIVE" == off ]]; then
    run_profile "$TEST_TOML" "$TEST_TUI" "$TEST_REST"
    exit 0
fi

echo
echo "==> Exhaustive feature-subset matrix"
for mask in 0 1 2 3 4 5 6 7; do
    if (( (mask & ~requested_mask) != 0 )); then
        continue
    fi

    toml=off
    tui=off
    rest=off
    (( (mask & 1) != 0 )) && toml=on
    (( (mask & 2) != 0 )) && tui=on
    (( (mask & 4) != 0 )) && rest=on
    run_profile "$toml" "$tui" "$rest"
done
