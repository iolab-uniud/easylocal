#!/usr/bin/env bash
# Measure the code coverage of the EasyLocal headers exercised by the test
# suite, and write text, HTML and Cobertura reports to build/coverage/.
#
#   scripts/coverage.sh                       # what the CI badge measures
#   CXX=g++-15 scripts/coverage.sh            # a given compiler
#   scripts/coverage.sh -DEASYLOCAL_ENABLE_REST=OFF ...   # extra CMake options
#
# The badge is measured with GCC, all optional components and gcovr from the
# uv environment (uv.lock); this script does the same by default, so the local
# figure matches it. Clang counts lines differently: with no GCC found it runs
# with the default compiler and says so.

set -euo pipefail

cd "$(dirname "$0")/.."
build_dir="build/coverage"

is_gcc() {
    "$1" --version 2>/dev/null | grep -q 'Free Software Foundation'
}

compiler="${CXX:-}"
if [ -z "$compiler" ]; then
    for candidate in g++-16 g++-15 g++-14 g++; do
        if command -v "$candidate" >/dev/null 2>&1 && is_gcc "$candidate"; then
            compiler="$(command -v "$candidate")"
            break
        fi
    done
fi

# The bundled dependencies, built with the same compiler and standard library:
# system packages (e.g. Homebrew's, built with Clang) may not link with GCC.
configure=(cmake --preset coverage
    -DEASYLOCAL_ENABLE_CONFIG_TOML=ON
    -DEASYLOCAL_ENABLE_TUI=ON
    -DEASYLOCAL_ENABLE_REST=ON
    -DEASYLOCAL_FETCH_DEPENDENCIES=ON
    -DCMAKE_DISABLE_FIND_PACKAGE_tomlplusplus=TRUE
    -DCMAKE_DISABLE_FIND_PACKAGE_ftxui=TRUE
    -DCMAKE_DISABLE_FIND_PACKAGE_Crow=TRUE)
if [ -n "$compiler" ]; then
    configure+=("-DCMAKE_CXX_COMPILER=$compiler")
    # A build directory configured for another compiler cannot be reused.
    cached="$(sed -n 's/^CMAKE_CXX_COMPILER:[A-Z]*=//p' "$build_dir/CMakeCache.txt" 2>/dev/null || true)"
    if [ -n "$cached" ] && [ "$cached" != "$compiler" ]; then
        configure+=(--fresh)
    fi
fi

"${configure[@]}" "$@"
cmake --build --preset coverage --parallel
# The objects of targets that no longer exist (a removed test, say) would be
# reported as code never run: drop them, and the .gcno notes beside them.
if command -v ninja >/dev/null 2>&1; then
    ninja -C "$build_dir" -t cleandead >/dev/null
fi
find "$build_dir" -name '*.gcno' | while read -r notes; do
    [ -e "${notes%.gcno}.o" ] || rm -f "$notes"
done
find "$build_dir" -name '*.gcda' -delete
ctest --preset coverage

compiler="$(sed -n 's/^CMAKE_CXX_COMPILER:[A-Z]*=//p' "$build_dir/CMakeCache.txt")"
if is_gcc "$compiler"; then
    gcov_tool="$(dirname "$compiler")/$(basename "$compiler" | sed 's/g++/gcov/')"
elif "$compiler" --version | grep -qi clang; then
    echo "note: measuring with Clang; the CI badge is measured with GCC and counts lines differently" >&2
    if command -v xcrun >/dev/null 2>&1; then
        gcov_tool="xcrun llvm-cov gcov"
    else
        gcov_tool="$(dirname "$compiler")/llvm-cov gcov"
    fi
else
    echo "unsupported compiler for coverage: $compiler" >&2
    exit 1
fi

if command -v uv >/dev/null 2>&1; then
    gcovr=(uv run --frozen gcovr)
else
    gcovr=(gcovr)  # 8 or later, for --merge-lines
fi

# Search only this build: gcovr would otherwise also read the .gcda files of
# any other build under the repository.
mkdir -p "$build_dir/html"
"${gcovr[@]}" "$build_dir" \
    --root . \
    --object-directory "$build_dir" \
    --gcov-executable "$gcov_tool" \
    --gcov-ignore-errors=no_working_dir_found \
    --filter 'include/easylocal/' \
    --exclude '.*/third_party/.*' \
    --exclude-unreachable-branches \
    --exclude-throw-branches \
    --merge-mode-functions separate \
    --merge-lines \
    --txt "$build_dir/coverage.txt" \
    --txt-summary \
    --html-details "$build_dir/html/index.html" \
    --cobertura "$build_dir/coverage.xml" \
    --json-summary "$build_dir/summary.json"

echo "reports: $build_dir/coverage.txt, $build_dir/html/index.html, $build_dir/coverage.xml, $build_dir/summary.json"
