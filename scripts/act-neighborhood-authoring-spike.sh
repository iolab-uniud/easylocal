#!/usr/bin/env bash

set -euo pipefail

die() {
    echo "ERROR: $*" >&2
    exit 1
}

usage() {
    cat <<'USAGE'
Usage:
  ./scripts/act-neighborhood-authoring-spike.sh
  ./scripts/act-neighborhood-authoring-spike.sh all [target_moves] [trials] [seed]
  ./scripts/act-neighborhood-authoring-spike.sh gcc15 [target_moves] [trials] [seed]
  ./scripts/act-neighborhood-authoring-spike.sh gcc16 [target_moves] [trials] [seed]
  ./scripts/act-neighborhood-authoring-spike.sh clang22-libstdcxx [target_moves] [trials] [seed]
  ./scripts/act-neighborhood-authoring-spike.sh clang22-libcxx [target_moves] [trials] [seed]

Runs the Ubuntu 26.04 Linux neighborhood-authoring workflow locally with act.
This is intended to validate the workflow and toolchain provisioning. Benchmark
timings from act are not authoritative when the host requires emulation.

The numeric target is used as approximate moves/trial for the traversal
benchmark and approximate evaluations/trial for the runner benchmark.

Defaults:
  target_moves = 1000000
  trials       = 3
  seed         = 123456789
USAGE
}

cd "$(dirname "$0")/.."

command -v act >/dev/null 2>&1 || die "'act' not found in PATH"
[[ -f .github/workflows/neighborhood-authoring-spike.yml ]] || \
    die ".github/workflows/neighborhood-authoring-spike.yml not found"

TARGET="${1:-all}"
TARGET_MOVES="${2:-1000000}"
TRIALS="${3:-3}"
SEED="${4:-123456789}"

[[ "$TARGET_MOVES" =~ ^[1-9][0-9]*$ ]] || die "target_moves must be positive"
[[ "$TRIALS" =~ ^[1-9][0-9]*$ ]] || die "trials must be positive"
[[ "$SEED" =~ ^[0-9]+$ ]] || die "seed must be an unsigned integer"

event_file="$(mktemp)"
trap 'rm -f "$event_file"' EXIT

cat > "$event_file" <<EOF_EVENT
{"inputs":{"target_moves":"$TARGET_MOVES","trials":"$TRIALS","seed":"$SEED"}}
EOF_EVENT

run_toolchain() {
    local toolchain="$1"

    echo
    echo "==> act spike: Ubuntu 26.04 / ${toolchain} / C++23"
    echo "    target_moves=${TARGET_MOVES}, trials=${TRIALS}, seed=${SEED}"
    echo

    act workflow_dispatch \
        -W .github/workflows/neighborhood-authoring-spike.yml \
        -j linux \
        --matrix "toolchain:${toolchain}" \
        --eventpath "$event_file"
}

case "$TARGET" in
    all)
        run_toolchain gcc15
        run_toolchain gcc16
        run_toolchain clang22-libstdcxx
        run_toolchain clang22-libcxx
        ;;
    gcc15|gcc16|clang22-libstdcxx|clang22-libcxx)
        run_toolchain "$TARGET"
        ;;
    -h|--help)
        usage
        ;;
    *)
        usage >&2
        exit 1
        ;;
esac
