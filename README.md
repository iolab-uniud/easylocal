# EasyLocal++

[![CI](https://github.com/iolab-uniud/easylocal-next/actions/workflows/ci.yml/badge.svg)](https://github.com/iolab-uniud/easylocal-next/actions/workflows/ci.yml)

EasyLocal++ is an incremental redesign of EasyLocal as a modern **C++23
header-only library** for local search and metaheuristics.

The project is being rebuilt from concrete minimal working examples, with tests
and the public API evolving incrementally from the contracts they expose.

> **Current status:** the public recipe-based `Runner`, deterministic n-ary
> neighborhood union, cursor-to-range neighborhood adapter, aggregation
> facilities, sampling vocabulary, and First/Best/Random First Improvement
> algorithms are in place. Domain models remain example-local while additional
> search/metaheuristic contracts are stabilized incrementally.

## Requirements

- C++23
- CMake >= 3.25
- Ninja
- GCC or Clang-family compiler

The current CI exercises:

- Linux: GCC 15, GCC 16, Clang 22 + libstdc++, and Clang 22 + libc++
- macOS ARM64: AppleClang and Homebrew GCC 16

macOS Intel is intentionally not part of the supported CI matrix.

## Local development on macOS

The primary development environment is macOS.

Prerequisites are Apple Command Line Tools or Xcode, CMake, and Ninja. With an
existing Homebrew installation:

```sh
brew install cmake ninja
```

Configure, build, and run the tests with:

```sh
cmake --preset dev
cmake --build --preset dev --parallel
ctest --preset dev --output-on-failure
```

For a local Release build:

```sh
cmake --preset release
cmake --build --preset release --parallel
ctest --preset release --output-on-failure
```

Build products and reports are kept under `build/<preset>/` and are ignored by
Git.

## Header-only library

EasyLocal++ is designed from the beginning as a header-only library.

The public CMake target is:

```cmake
EasyLocal::EasyLocal
```

A configured build tree can be installed to any prefix:

```sh
cmake --install build/release --prefix /path/to/easylocal-prefix
```

An external CMake project can then consume that installation with:

```cmake
find_package(EasyLocal CONFIG REQUIRED)

add_executable(my_search main.cpp)
target_link_libraries(my_search PRIVATE EasyLocal::EasyLocal)
```

Point `CMAKE_PREFIX_PATH` at the chosen installation prefix when it is not in a
standard CMake search location. The imported target propagates the installed
include directory and the C++23 compile requirement; consumers do not need to
add EasyLocal include paths manually.

Consumers use the public headers under:

```text
include/easylocal/
```

The test suite checks header self-containment and multi-translation-unit use to
catch ODR issues that are particularly relevant to header-only libraries. It
also installs EasyLocal into an isolated prefix, configures a separate consumer
with `find_package(EasyLocal)`, builds it through `EasyLocal::EasyLocal`, and
runs the resulting executable.

EasyLocal also provides optional, non-virtual convenience bases for the common
service boilerplate:

```cpp
class MySolutionManager
    : public easylocal::solution_manager_base<Instance, Solution>
{
public:
    using solution_manager_base::solution_manager_base;
    // domain-specific validity/evaluation behavior
};

class MyNeighborhoodExplorer
    : public easylocal::neighborhood_explorer_base<MySolutionManager, Move>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;
    // domain-specific move generation/application behavior
};
```

The bases only provide associated type aliases and reference plumbing.
`solution_manager_base` stores the bound `Instance` reference, while
`neighborhood_explorer_base` stores only the `SolutionManager` reference and
derives its instance through that manager. They do not use virtual dispatch and
are not required by the structural Runner concepts; fully custom duck-typed
services remain supported.

The current Assignment and TSP MWEs live under `examples/assignment/` and
`examples/tsp/` and are intentionally not part of the public include tree.

Experimental search/metaheuristic work lives under `examples/search/`. Those
headers are an incubator used to pressure-test framework contracts and are not
public EasyLocal++ API.

## Continuous integration

The full CI matrix is intentionally small and targets C++23 directly:

| Platform | Toolchain |
| --- | --- |
| Ubuntu 26.04 | GCC 15 |
| Ubuntu 26.04 | GCC 16 |
| Ubuntu 26.04 | Clang 22 + libstdc++ |
| Ubuntu 26.04 | Clang 22 + libc++ |
| macOS ARM64 | AppleClang |
| macOS ARM64 | GCC 16 |

GitHub Actions runs automatically for pull requests and release tags of the form
`vX.Y.Z`, and can also be started manually with `workflow_dispatch`.

Normal development pushes do not trigger the remote CI automatically.

Linux CI jobs can be exercised locally with `act`:

```sh
./scripts/act-ci.sh
```

or for a single toolchain:

```sh
./scripts/act-ci.sh gcc15
./scripts/act-ci.sh gcc16
./scripts/act-ci.sh clang22-libstdcxx
./scripts/act-ci.sh clang22-libcxx
```

## Performance benchmarks

A small opt-in benchmark suite under `benchmarks/neighborhood_traversal/` tracks
performance of the neighborhood traversal abstractions that remain part of the
design: raw First/Next as an oracle, the public cursor-to-range adapter, and
coroutine-backed ranges. `std::generator` is included when the active standard
library provides it.

Run it locally with:

```sh
./scripts/run-neighborhood-benchmarks.sh \
    build/neighborhood-benchmark-results \
    5000000 5 123456789
```

The benchmark checks semantic equivalence before timing and reports diagnostic
ratios only. It deliberately has no automatic performance pass/fail threshold.
Authoritative cross-toolchain measurements use the manual **Neighborhood Benchmarks** GitHub Actions workflow.

## Tests

The current tests cover:

- toolchain/C++23 support;
- public-header self-containment;
- multi-translation-unit linking;
- the assignment MWE model contract, including value semantics, structural
  validity, full evaluation, and coexistence of managers bound to different
  instances;
- deterministic range-based neighborhood traversal, move application, and
  composition with standard range filters;
- deterministic n-ary neighborhood union, including heterogeneous move types
  and transparent use through the public `Runner`;
- static random traversal semantics (`with_replacement` / `without_replacement`)
  and deterministic seeded random traversal;
- public First, Best, and Random First Improvement integration through the Runner.

CTest is the common test entry point locally and in CI:

```sh
ctest --preset dev --output-on-failure
```

Tests are named after stable contracts/responsibilities rather than development
iterations. They evolve with the design and are removed only when the contract
they verify is deliberately abandoned.

## Versioning and releases

`VERSION` is the single source of truth for the EasyLocal++ version.

Releases use semantic versioning and annotated tags of the form:

```text
vMAJOR.MINOR.PATCH
```

The release helper updates `VERSION`, prepares a `CHANGELOG.md` entry from the
Git history, optionally asks `claude -p` to draft it, opens it for manual
review, runs the local Release build and CTest suite, and finally creates and
pushes the release tag:

```sh
./scripts/release.sh patch
./scripts/release.sh minor
./scripts/release.sh major
```

The pushed tag triggers the full GitHub Actions CI matrix.

## Legacy reference

The legacy reference is Bitbucket `satt/easylocal-3`, branch `no_output`,
reviewed at commit `b40b14c2db2bdc81574a0613c52674644f8a0101`.

The unfinished GitHub redesign is **not** the architectural baseline.
