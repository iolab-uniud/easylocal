#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${SPIKE_BUILD_DIR:-${repo_root}/build/neighborhood-authoring-spike}"
if [[ "${build_dir}" != /* ]]; then
    build_dir="${repo_root}/${build_dir}"
fi
target_evaluations="${1:-2000000}"
trials="${2:-3}"
seed="${3:-123456789}"

cmake_args=()
if [[ -n "${SDKROOT:-}" ]]; then
    cmake_args+=("-DCMAKE_OSX_SYSROOT=${SDKROOT}")
fi

cmake \
    -S "${repo_root}" \
    -B "${build_dir}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DEASYLOCAL_BUILD_EXAMPLES=OFF \
    -DEASYLOCAL_BUILD_TESTS=OFF \
    -DEASYLOCAL_BUILD_SPIKES=ON \
    "${cmake_args[@]}"

cmake --build "${build_dir}" --target \
    easylocal_neighborhood_runner_benchmark

"${build_dir}/spikes/neighborhood_authoring/easylocal_neighborhood_runner_benchmark" \
    "${target_evaluations}" \
    "${trials}" \
    "${seed}"
