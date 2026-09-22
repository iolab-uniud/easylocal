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
  ./scripts/act-ci.sh gcc14
  ./scripts/act-ci.sh clang18

By default, runs the Linux GitHub Actions job locally with act for both
supported toolchains, using the locally cached runner image when available.
USAGE
}

cd "$(dirname "$0")/.."

command -v act >/dev/null 2>&1 || die "'act' not found in PATH"
[[ -f .github/workflows/ci.yml ]] || die ".github/workflows/ci.yml not found"

TARGET="${1:-all}"

run_toolchain() {
    local toolchain="$1"

    echo
    echo "==> act: Linux / ${toolchain} / C++23"
    echo

    act workflow_dispatch \
        -W .github/workflows/ci.yml \
        -j linux \
        --matrix "toolchain:${toolchain}" \
        --pull=false
}

case "$TARGET" in
    all)
        run_toolchain gcc14
        run_toolchain clang18
        ;;
    gcc14|clang18)
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
