#!/usr/bin/env bash
# clang-format with the repository's .clang-format, from the uv environment
# (the version is pinned in pyproject.toml).
#
#   scripts/format.sh             # format the lines changed since HEAD, and examples/
#   scripts/format.sh --staged    # check the staged lines (the pre-commit hook)
#   scripts/format.sh --check [base]
#                                 # check examples/ and the lines changed since
#                                 # <base> (default HEAD): what CI runs
#
# Only changed lines are touched outside examples/: the library and the tests
# take the style as they are edited, not in one sweeping reformat.

set -euo pipefail

cd "$(dirname "$0")/.."

mode="${1:-fix}"
base="${2:-HEAD}"

examples() {
    git ls-files 'examples/*.hpp' 'examples/*.cpp'
}

# git clang-format prints this when the selected lines are already formatted.
clean_output() {
    grep -qE '^(no modified files to format|clang-format did not modify any files)$'
}

case "$mode" in
    fix)
        examples | xargs uv run clang-format -i
        uv run git-clang-format --quiet --force HEAD -- '*.hpp' '*.cpp' || true
        ;;
    --staged)
        diff="$(uv run git-clang-format --staged --diff -- '*.hpp' '*.cpp' || true)"
        if [ -n "$diff" ] && ! clean_output <<<"$diff"; then
            printf '%s\n' "$diff"
            echo >&2
            echo "The staged changes are not formatted. Run scripts/format.sh," >&2
            echo "stage the result and commit again." >&2
            exit 1
        fi
        ;;
    --check)
        status=0
        examples | xargs uv run clang-format --dry-run -Werror || status=1
        diff="$(uv run git-clang-format --diff "$base" -- '*.hpp' '*.cpp' || true)"
        if [ -n "$diff" ] && ! clean_output <<<"$diff"; then
            printf '%s\n' "$diff"
            status=1
        fi
        if [ "$status" -ne 0 ]; then
            echo "Formatting differs from .clang-format: run scripts/format.sh." >&2
        fi
        exit "$status"
        ;;
    *)
        sed -n '2,12p' "$0" >&2
        exit 2
        ;;
esac
