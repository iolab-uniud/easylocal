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

if ! command -v uv >/dev/null 2>&1; then
    echo "uv not found: install it (https://docs.astral.sh/uv/), then run uv sync." >&2
    exit 2
fi

examples() {
    git ls-files 'examples/*.hpp' 'examples/*.cpp'
}

# git clang-format with the arguments, its output in $diff. It exits with 0
# when the lines are formatted, 1 when it changed or would change them, 2 on
# an error: the script stops on an error instead of taking it for clean.
git_clang_format() {
    local code=0
    diff="$(uv run git-clang-format "$@" 2>&1)" || code=$?
    if [ "$code" -gt 1 ]; then
        printf '%s\n' "$diff" >&2
        echo "git-clang-format failed with exit status $code." >&2
        exit 2
    fi
    return "$code"
}

case "$mode" in
    fix)
        examples | xargs uv run clang-format -i
        git_clang_format --quiet --force HEAD -- '*.hpp' '*.cpp' || true
        ;;
    --staged)
        if ! git_clang_format --staged --diff -- '*.hpp' '*.cpp'; then
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
        if ! git_clang_format --diff "$base" -- '*.hpp' '*.cpp'; then
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
