#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
results_dir="${1:-${repo_root}/build/neighborhood-benchmark-results}"
target_work="${2:-2000000}"
trials="${3:-3}"
seed="${4:-123456789}"

if [[ "${results_dir}" != /* ]]; then
    results_dir="${repo_root}/${results_dir}"
fi

if [[ ! "${target_work}" =~ ^[1-9][0-9]*$ ]]; then
    echo "error: target_work must be a positive integer" >&2
    exit 2
fi
if [[ ! "${trials}" =~ ^[1-9][0-9]*$ ]]; then
    echo "error: trials must be a positive integer" >&2
    exit 2
fi
if [[ ! "${seed}" =~ ^[0-9]+$ ]]; then
    echo "error: seed must be an unsigned integer" >&2
    exit 2
fi

mkdir -p "${results_dir}"

toolchain_tag="${TOOLCHAIN:-local}"
build_dir="${BENCHMARK_BUILD_DIR:-build/neighborhood-benchmark-${toolchain_tag}}"
if [[ "${build_dir}" != /* ]]; then
    build_dir="${repo_root}/${build_dir}"
fi

cmake_args=()
if [[ -n "${SDKROOT:-}" ]]; then
    cmake_args+=("-DCMAKE_OSX_SYSROOT=${SDKROOT}")
fi

{
    cmake \
        -S "${repo_root}" \
        -B "${build_dir}" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DEASYLOCAL_BUILD_EXAMPLES=OFF \
        -DEASYLOCAL_BUILD_TESTS=OFF \
        -DEASYLOCAL_BUILD_BENCHMARKS=ON \
        "${cmake_args[@]}"

    cmake --build "${build_dir}" --target \
        easylocal_neighborhood_traversal_benchmark \
        easylocal_neighborhood_search_benchmark
} > "${results_dir}/build.log" 2>&1

python3 "${repo_root}/scripts/neighborhood-benchmark-metadata.py" \
    --output "${results_dir}/metadata.csv" \
    --repo-root "${repo_root}" \
    --target-work "${target_work}" \
    --trials "${trials}" \
    --seed "${seed}"

"${build_dir}/benchmarks/neighborhood_traversal/easylocal_neighborhood_traversal_benchmark" \
    "${target_work}" \
    "${trials}" \
    "${seed}" \
    > "${results_dir}/traversal.csv" \
    2> "${results_dir}/traversal.log"

"${build_dir}/benchmarks/neighborhood_traversal/easylocal_neighborhood_search_benchmark" \
    "${target_work}" \
    "${trials}" \
    "${seed}" \
    > "${results_dir}/search.csv" \
    2> "${results_dir}/search.log"

python3 "${repo_root}/scripts/summarize-neighborhood-benchmarks.py" \
    "${results_dir}" \
    --output "${results_dir}/summary.md"

printf 'Neighborhood benchmark results: %s\n' "${results_dir}" >&2
