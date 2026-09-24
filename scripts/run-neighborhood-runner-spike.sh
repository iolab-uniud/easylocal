#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
target_evaluations="${1:-2000000}"
trials="${2:-3}"
seed="${3:-123456789}"
compiler_selector="${4:-}"

resolve_compiler() {
    local selector="$1"
    local candidate=""

    case "${selector}" in
        appleclang)
            if ! command -v xcrun >/dev/null 2>&1; then
                echo "error: xcrun is required for compiler selector 'appleclang'" >&2
                return 1
            fi
            candidate="$(xcrun -f clang++)"
            ;;
        gcc15)
            if command -v g++-15 >/dev/null 2>&1; then
                candidate="$(command -v g++-15)"
            elif command -v brew >/dev/null 2>&1; then
                local brew_prefix
                brew_prefix="$(brew --prefix gcc@15 2>/dev/null || true)"
                if [[ -n "${brew_prefix}" ]]; then
                    candidate="${brew_prefix}/bin/g++-15"
                fi
            fi
            ;;
        gcc16)
            if command -v g++-16 >/dev/null 2>&1; then
                candidate="$(command -v g++-16)"
            elif command -v brew >/dev/null 2>&1; then
                local brew_prefix
                brew_prefix="$(brew --prefix gcc 2>/dev/null || true)"
                if [[ -n "${brew_prefix}" ]]; then
                    candidate="${brew_prefix}/bin/g++-16"
                fi
            fi
            ;;
        "")
            if [[ -n "${CXX:-}" ]]; then
                selector="${CXX}"
            else
                selector="c++"
            fi
            if [[ "${selector}" == */* ]]; then
                candidate="${selector}"
            else
                candidate="$(command -v "${selector}" 2>/dev/null || true)"
            fi
            ;;
        *)
            if [[ "${selector}" == */* ]]; then
                candidate="${selector}"
            else
                candidate="$(command -v "${selector}" 2>/dev/null || true)"
            fi
            ;;
    esac

    if [[ -z "${candidate}" || ! -x "${candidate}" ]]; then
        echo "error: unable to resolve C++ compiler '${selector}'" >&2
        echo "       supported shortcuts: appleclang, gcc15, gcc16" >&2
        return 1
    fi

    printf '%s\n' "${candidate}"
}

compiler_path="$(resolve_compiler "${compiler_selector}")"
compiler_banner="$("${compiler_path}" --version 2>/dev/null | head -n 1 || true)"
compiler_basename="$(basename "${compiler_path}")"

compiler_tag=""
case "${compiler_selector}" in
    appleclang|gcc15|gcc16)
        compiler_tag="${compiler_selector}"
        ;;
esac

if [[ -z "${compiler_tag}" ]]; then
    if [[ "${compiler_banner}" == *"Apple clang"* ]]; then
        compiler_tag="appleclang"
    elif [[ "${compiler_basename}" =~ ^g\+\+-([0-9]+)$ ]]; then
        compiler_tag="gcc${BASH_REMATCH[1]}"
    elif [[ "${compiler_basename}" == "g++" ]]; then
        gcc_major="$("${compiler_path}" -dumpversion 2>/dev/null | cut -d. -f1 || true)"
        compiler_tag="gcc${gcc_major:-unknown}"
    elif [[ "${compiler_banner}" == *"clang version"* || "${compiler_banner}" == *"Ubuntu clang version"* ]]; then
        clang_major="$(printf '%s\n' "${compiler_banner}" | sed -E 's/.*clang version ([0-9]+).*/\1/' || true)"
        if [[ "${clang_major}" =~ ^[0-9]+$ ]]; then
            compiler_tag="clang${clang_major}"
        else
            compiler_tag="clang"
        fi
    else
        compiler_tag="$(printf '%s' "${compiler_basename}" | tr -cs '[:alnum:]._- ' '-' | tr ' ' '-')"
        compiler_tag="${compiler_tag%-}"
    fi
fi

build_dir="${SPIKE_BUILD_DIR:-${repo_root}/build/neighborhood-runner-${compiler_tag}}"
if [[ "${build_dir}" != /* ]]; then
    build_dir="${repo_root}/${build_dir}"
fi

cmake_args=()
if [[ -n "${SDKROOT:-}" ]]; then
    cmake_args+=("-DCMAKE_OSX_SYSROOT=${SDKROOT}")
fi

printf 'runner compiler: %s\n' "${compiler_path}" >&2
printf 'runner compiler version: %s\n' "${compiler_banner:-unknown}" >&2
printf 'runner build dir: %s\n' "${build_dir}" >&2

CXX="${compiler_path}" cmake \
    -S "${repo_root}" \
    -B "${build_dir}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DEASYLOCAL_BUILD_EXAMPLES=OFF \
    -DEASYLOCAL_BUILD_TESTS=OFF \
    -DEASYLOCAL_BUILD_SPIKES=ON \
    "${cmake_args[@]}" \
    >&2

cmake --build "${build_dir}" --target \
    easylocal_neighborhood_runner_benchmark \
    >&2

"${build_dir}/spikes/neighborhood_authoring/easylocal_neighborhood_runner_benchmark" \
    "${target_evaluations}" \
    "${trials}" \
    "${seed}"
