# EasyLocal next: S0a

Incremental rebuild of EasyLocal++. This snapshot contains **only development
infrastructure**, not a new framework implementation.

**Status:** files authored; configuration, compilation, CTest, act and GitHub
Actions have not been executed for this snapshot. Run them on the developer's
Mac and in the configured CI. There are no inherited test results from earlier
chat-generated bootstraps.

## Local development on macOS

Prerequisites: Apple Command Line Tools or Xcode, CMake >= 3.25, and Ninja.
With an existing Homebrew installation, missing build tools can be installed with
`brew install cmake ninja`. The local preset uses `/usr/bin/clang++` (AppleClang).
It does not require Homebrew GCC, Docker or act.

```sh
cmake --preset dev
cmake --build --preset dev --parallel 2
ctest --preset dev
```

For native C++20 Release, use `release` instead of `dev` in all three commands.
Build products and reports are under `build/<preset>/`, ignored by Git.

Read [the CI guide](docs/build-ci.md) before running act or enabling remote CI.
The workflow expects `main` for pushes, any pull request, or manual dispatch.

## Scope

- Linux: GCC 14 / libstdc++ 14 and Clang 18 / libstdc++ 14.
- macOS ARM64 only: AppleClang / libc++ and Homebrew GCC 14 / libstdc++ 14.
- Each remote toolchain: C++20 Debug and C++23 Release.
- Local act: Linux x86_64 containers only, selected with `-j linux`.

C++23 mode here is a compatibility probe for the same C++20 source; it does not
certify complete C++23 language or library support. Compiler versions are initial
CI samples, not a declaration of the framework's minimum supported versions.

## What the tests mean

`infrastructure.cpp20` compiles and runs a small concepts/ranges/span smoke check
and prints the compiler, language macro and standard-library header version.
`infrastructure.failure-signal` uses a deliberate nonzero return and CTest's
`WILL_FAIL` property. Its **passing** result is expected. No test relies on
`assert`, so its checks are not removed in Release. An empty suite is an error.

These are infrastructure checks, not solver tests, numerical tests or performance
measurements. The first meaningful problem-specific tests arrive with the MWE.

## Project records

- [Plan and proposed MWE](docs/plan.md).
- [Accepted decisions and open proposals](docs/decisions.md).
- [EL3 migration record](docs/migration-el3.md).

Legacy reference: Bitbucket `satt/easylocal-3`, branch `no_output`, reviewed commit
`b40b14c2db2bdc81574a0613c52674644f8a0101`. The unfinished GitHub design is not the
architectural baseline. No legacy code or third-party source is bundled here.
