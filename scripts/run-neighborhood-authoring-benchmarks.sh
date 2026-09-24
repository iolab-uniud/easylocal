#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
results_dir="${1:-${repo_root}/build/neighborhood-authoring-results}"
target_moves="${2:-2000000}"
trials="${3:-3}"
seed="${4:-123456789}"

if [[ "${results_dir}" != /* ]]; then
    results_dir="${repo_root}/${results_dir}"
fi

if [[ ! "${target_moves}" =~ ^[1-9][0-9]*$ ]]; then
    echo "error: target_moves must be a positive integer" >&2
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

build_dir="${SPIKE_BUILD_DIR:-}"
if [[ -z "${build_dir}" ]]; then
    toolchain_tag="${TOOLCHAIN:-local}"
    build_dir="build/neighborhood-authoring-spike-${toolchain_tag}"
fi

python3 "${repo_root}/scripts/neighborhood-benchmark-metadata.py" \
    --output "${results_dir}/metadata.csv" \
    --repo-root "${repo_root}" \
    --target-moves "${target_moves}" \
    --trials "${trials}" \
    --seed "${seed}"

SPIKE_BUILD_DIR="${build_dir}" \
SPIKE_ALLOCATION_REPORT="${results_dir}/allocations.csv" \
    "${repo_root}/scripts/run-neighborhood-authoring-spike.sh" \
        "${target_moves}" \
        "${trials}" \
        "${seed}" \
        > "${results_dir}/benchmark.csv" \
        2> "${results_dir}/check.log"

SPIKE_BUILD_DIR="${build_dir}" \
    "${repo_root}/scripts/run-neighborhood-runner-spike.sh" \
        "${target_moves}" \
        "${trials}" \
        "${seed}" \
        > "${results_dir}/runner-benchmark.csv" \
        2> "${results_dir}/runner-check.log"

normalized_build_dir="${build_dir}"
if [[ "${normalized_build_dir}" != /* ]]; then
    normalized_build_dir="${repo_root}/${normalized_build_dir}"
fi

cmake --build "${normalized_build_dir}" --target \
    easylocal_neighborhood_first_improvement_diagnostic \
    > "${results_dir}/first-improvement-diagnostic.log" 2>&1

"${normalized_build_dir}/spikes/neighborhood_authoring/easylocal_neighborhood_first_improvement_diagnostic" \
    "${target_moves}" \
    "${trials}" \
    "${seed}" \
    > "${results_dir}/first-improvement-diagnostic.csv" \
    2>> "${results_dir}/first-improvement-diagnostic.log"

python3 "${repo_root}/scripts/summarize-neighborhood-authoring.py" \
    "${results_dir}" \
    --output "${results_dir}/summary.md"

printf 'Neighborhood benchmark results: %s\n' "${results_dir}" >&2
