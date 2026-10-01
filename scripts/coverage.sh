#!/usr/bin/env bash
# Measure the code coverage of the EasyLocal headers exercised by the test
# suite, and write text, HTML and Cobertura reports to build/coverage/.
#
#   scripts/coverage.sh                       # Core only
#   scripts/coverage.sh -DEASYLOCAL_ENABLE_TUI=ON ...   # extra CMake options
#
# Uses the `coverage` preset (Debug, --coverage) and gcovr, from the uv
# environment when uv is available.

set -euo pipefail

cd "$(dirname "$0")/.."
build_dir="build/coverage"

cmake --preset coverage "$@"
cmake --build --preset coverage --parallel
find "$build_dir" -name '*.gcda' -delete
ctest --preset coverage

compiler="$(sed -n 's/^CMAKE_CXX_COMPILER:[A-Z]*=//p' "$build_dir/CMakeCache.txt")"
if "$compiler" --version | grep -qi clang; then
    compiler_id=Clang
elif "$compiler" --version | grep -qiE 'g\+\+|gcc'; then
    compiler_id=GNU
else
    compiler_id=unknown
fi
case "$compiler_id" in
    Clang)
        if command -v xcrun >/dev/null 2>&1; then
            gcov_tool="xcrun llvm-cov gcov"
        else
            gcov_tool="$(dirname "$compiler")/llvm-cov gcov"
        fi
        ;;
    GNU)
        gcov_tool="$(dirname "$compiler")/$(basename "$compiler" | sed 's/g++/gcov/')"
        ;;
    *)
        echo "unsupported compiler for coverage: $compiler_id" >&2
        exit 1
        ;;
esac

if command -v uv >/dev/null 2>&1; then
    gcovr=(uv run gcovr)
else
    gcovr=(gcovr)
fi

mkdir -p "$build_dir/html"
"${gcovr[@]}" \
    --root . \
    --object-directory "$build_dir" \
    --gcov-executable "$gcov_tool" \
    --filter 'include/easylocal/' \
    --exclude '.*/third_party/.*' \
    --exclude-unreachable-branches \
    --exclude-throw-branches \
    --merge-mode-functions separate \
    --txt "$build_dir/coverage.txt" \
    --txt-summary \
    --html-details "$build_dir/html/index.html" \
    --cobertura "$build_dir/coverage.xml" \
    --json-summary "$build_dir/summary.json"

echo "reports: $build_dir/coverage.txt, $build_dir/html/index.html, $build_dir/coverage.xml, $build_dir/summary.json"
