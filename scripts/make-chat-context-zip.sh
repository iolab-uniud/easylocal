#!/usr/bin/env bash
set -euo pipefail

# Create a compact EasyLocal++ source snapshot for handing the repository to a
# new ChatGPT conversation. The archive intentionally excludes build trees,
# compiler outputs, CMake caches, benchmark/result artifacts, VCS internals,
# editor caches and previously generated archives.
#
# Usage:
#   ./scripts/make-chat-context-zip.sh
#   ./scripts/make-chat-context-zip.sh /path/to/easylocal-next-chat.zip
#
# Run this script from anywhere inside the repository.

if ! command -v git >/dev/null 2>&1; then
    echo "error: git is required" >&2
    exit 1
fi

if ! command -v zip >/dev/null 2>&1; then
    echo "error: zip is required" >&2
    exit 1
fi

ROOT="$(git rev-parse --show-toplevel 2>/dev/null)" || {
    echo "error: not inside a Git repository" >&2
    exit 1
}

PROJECT_NAME="$(basename "$ROOT")"
STAMP="$(date '+%Y%m%d-%H%M%S')"
DEFAULT_OUT="$(dirname "$ROOT")/${PROJECT_NAME}-chat-${STAMP}.zip"
OUT="${1:-$DEFAULT_OUT}"

case "$OUT" in
    /*) ;;
    *) OUT="$PWD/$OUT" ;;
esac

mkdir -p "$(dirname "$OUT")"

TMPDIR_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/easylocal-chat-zip.XXXXXX")"
trap 'rm -rf "$TMPDIR_ROOT"' EXIT

TMP_ZIP="$TMPDIR_ROOT/${PROJECT_NAME}-chat.zip"

cd "$ROOT"

zip -rq "$TMP_ZIP" . \
    -x '.git/*' \
       '.git' \
       '.DS_Store' \
       '*/.DS_Store' \
       '.idea/*' \
       '.vscode/*' \
       '.cache/*' \
       '*/.cache/*' \
       '.pytest_cache/*' \
       '*/.pytest_cache/*' \
       '__pycache__/*' \
       '*/__pycache__/*' \
       'build/*' \
       'build-*/*' \
       'build_*/*' \
       'cmake-build-*/*' \
       'out/*' \
       '_build/*' \
       '*/CMakeFiles/*' \
       '*/CMakeCache.txt' \
       '*/cmake_install.cmake' \
       '*/CTestTestfile.cmake' \
       '*/compile_commands.json' \
       '*/Testing/*' \
       '*.o' \
       '*.obj' \
       '*.lo' \
       '*.a' \
       '*.so' \
       '*.so.*' \
       '*.dylib' \
       '*.dll' \
       '*.exe' \
       '*.out' \
       '*.gcda' \
       '*.gcno' \
       '*.gcov' \
       '*.profraw' \
       '*.profdata' \
       '*.dSYM/*' \
       '*.tmp' \
       '*.swp' \
       '*~' \
       'artifacts/*' \
       'benchmark-artifacts/*' \
       'benchmark-results/*' \
       'results/benchmarks/*' \
       '*.zip' \
       '*.tar' \
       '*.tar.gz' \
       '*.tgz' \
       '*.tar.xz'

# The destination may live inside the repository. Building in a temporary
# directory prevents the archive from recursively including itself.
mv "$TMP_ZIP" "$OUT"

echo "Created: $OUT"
echo
echo "Archive summary:"
unzip -l "$OUT" | tail -n 2

echo
echo "Handoff files:"
for handoff_file in \
    '.local/context.md' \
    '.local/decisions.md' \
    '.local/migration-el3.md'
do
    if unzip -Z1 "$OUT" | grep -qx "$handoff_file"; then
        echo "  OK  $handoff_file"
    else
        echo "  WARN $handoff_file is missing"
    fi
done
