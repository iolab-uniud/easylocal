#!/usr/bin/env bash

set -euo pipefail

die() {
    echo "ERROR: $*" >&2
    exit 1
}

usage() {
    cat <<'USAGE'
Usage:
  ./scripts/act-ci.sh
  ./scripts/act-ci.sh all
  ./scripts/act-ci.sh gcc15
  ./scripts/act-ci.sh gcc16
  ./scripts/act-ci.sh clang22-libstdcxx
  ./scripts/act-ci.sh clang22-libcxx

By default, runs the Ubuntu 26.04 Linux GitHub Actions job locally with act for
all supported Linux toolchains. act is a functional workflow check only; do not
use its timings as benchmark results on hosts that require container emulation.
USAGE
}

cd "$(dirname "$0")/.."

command -v act >/dev/null 2>&1 || die "'act' not found in PATH"
[[ -f .github/workflows/ci.yml ]] || die ".github/workflows/ci.yml not found"

TARGET="${1:-all}"

run_toolchain() {
    local toolchain="$1"

    echo
    echo "==> act: Ubuntu 26.04 / ${toolchain} / C++23"
    echo

    # A native multi-arch Ubuntu 26.04 image for the GitHub runner label; no
    # forced container architecture, so Docker picks the host's (ARM64 on
    # Apple Silicon).
    act workflow_dispatch \
        -P ubuntu-26.04=ghcr.io/harryzcy/ubuntu:26.04 \
        -W .github/workflows/ci.yml \
        -j linux \
        --matrix "toolchain:${toolchain}"
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
