#!/usr/bin/env bash
# clang-tidy with the repository's .clang-tidy on the example programs, from
# the uv environment (the version is pinned in pyproject.toml).
#
#   scripts/tidy.sh [build-dir]    # default build/dev
#
# The build directory provides compile_commands.json (every preset exports
# it); configure it with the optional components to check their examples too.

set -euo pipefail

cd "$(dirname "$0")/.."
build_dir="${1:-build/dev}"

if [ ! -f "$build_dir/compile_commands.json" ]; then
    echo "missing $build_dir/compile_commands.json: configure $build_dir first" >&2
    exit 2
fi

extra=()
if [ "$(uname)" = Darwin ]; then
    # The clang-tidy wheel does not know where the macOS SDK lives.
    extra=(--extra-arg=-isysroot "--extra-arg=$(xcrun --show-sdk-path)")
fi

# The examples' sources that this build compiles.
files=()
while IFS= read -r file; do
    if grep -q "\"file\": \"$PWD/$file\"" "$build_dir/compile_commands.json"; then
        files+=("$file")
    fi
done < <(git ls-files 'examples/*.cpp')

# ${files[@]+...}: an empty array is unbound for the bash 3.2 of macOS.
if [ -z "${files[*]+set}" ]; then
    echo "no example is compiled in $build_dir" >&2
    exit 0
fi
printf '%s\n' "${files[@]}" \
    | xargs -P "$(getconf _NPROCESSORS_ONLN)" -n 1 \
        uv run clang-tidy --quiet -p "$build_dir" ${extra[@]+"${extra[@]}"}
