# EasyLocal++

[![CI](https://github.com/iolab-uniud/easylocal-next/actions/workflows/ci.yml/badge.svg)](https://github.com/iolab-uniud/easylocal-next/actions/workflows/ci.yml)

EasyLocal++ is an incremental redesign of EasyLocal as a modern **C++23
header-only library** for local search and metaheuristics.

The project is being rebuilt from concrete minimal working examples, with tests
and the public API evolving incrementally from the contracts they expose.

> **Current status:** infrastructure bootstrap complete; the public recipe-based
> `Runner` and deterministic n-ary neighborhood union facilities have been
> extracted from the assignment MWE, while domain model and search algorithms
> remain example-local.

## Requirements

- C++23
- CMake >= 3.25
- Ninja
- GCC or Clang-family compiler

The current CI exercises:

- Linux: GCC 14 and Clang 18
- macOS ARM64: AppleClang and Homebrew GCC 14

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

Consumers will use the public headers under:

```text
include/easylocal/
```

The test suite also checks header self-containment and multi-translation-unit
use to catch ODR issues that are particularly relevant to header-only
libraries.

The current assignment MWE lives under `examples/assignment/` and is
intentionally not part of the public include tree.

## Continuous integration

The full CI matrix is intentionally small and targets C++23 directly:

| Platform | Toolchain |
| --- | --- |
| Ubuntu 24.04 | GCC 14 |
| Ubuntu 24.04 | Clang 18 |
| macOS ARM64 | AppleClang |
| macOS ARM64 | GCC 14 |

GitHub Actions runs automatically for release tags of the form `vX.Y.Z` and can
also be started manually with `workflow_dispatch`.

Normal development pushes do not trigger the remote CI automatically.

Linux CI jobs can be exercised locally with `act`:

```sh
./scripts/act-ci.sh
```

or for a single toolchain:

```sh
./scripts/act-ci.sh gcc14
./scripts/act-ci.sh clang18
```

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
  and transparent use through the public `Runner`.

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
