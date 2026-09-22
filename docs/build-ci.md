# S0a: development and CI

Prepared 2026-09-22. **Execution status: NOT RUN.**

## The first local run

Use the new directory as a normal Git working tree, not a copy of a previous
build directory. The archive has no `.git` history, generated builds or binaries.

```sh
cd /path/to/easylocal-next
# Only when this is a new, uninitialized repository:
git init -b main

cmake --preset dev
cmake --build --preset dev --parallel 2
ctest --preset dev
```

Use the three commands separately initially: the failing phase then remains
obvious. `ctest --preset dev --verbose` includes the toolchain fingerprint even
when tests pass. The deliberate `failure-signal` test must report Passed.

Apple tools must already be installed/selected. When they are missing,
`xcode-select --install` starts Apple's interactive installer. With Homebrew
already installed, `brew install cmake ninja` supplies the build tools.
There is no need to install every CI compiler on the development Mac.

After reviewing the files and local result:

```sh
git add CMakeLists.txt CMakePresets.json README.md .gitignore .actrc .github tests docs
git commit -m "build: add CMake CTest and Linux/macOS ARM64 CI"
```

Create an empty GitHub repository under the account/organization you choose, then
add its actual remote URL and push `main`. No remote has been created, modified or
assumed by this package. Avoid initializing that remote with a separate README
when importing this first local commit. Branch protection is not configured here.

## Shared presets

| Preset | Environment | Compiler | Standard | Build |
|---|---|---|---|---|
| dev | Local macOS | /usr/bin/clang++ | 20 | Debug |
| release | Local macOS | /usr/bin/clang++ | 20 | Release |
| ci20-debug | CI / act | Explicit CXX | 20 | Debug |
| ci23-release | CI / act | Explicit CXX | 23 | Release |

`CMakeUserPresets.json` is ignored. Put machine-specific compilers and paths there,
with **distinct binary directories for different toolchains**. For example, a
local GNU preset may inherit `_base`, set `CMAKE_CXX_COMPILER` to the actual
Homebrew `g++-14` path and supply GNU identity checks. Do not switch compilers in
an existing build tree. CI presets are configured by the workflow's environment;
they are not the default local-development entry point.

Language mode is required, extensions are disabled, and C++17 is rejected.
Warnings are enabled on the smoke executable only. `-Werror` is on in CI and off
in ordinary local presets; it is not exported as a consumer requirement.
No `fast-math`, sanitizer, test framework, package manager or parallel algorithm
backend is introduced in this increment.

## Remote matrix

| Runner | Toolchain | Library | Profiles |
|---|---|---|---|
| ubuntu-24.04 / x86_64 | GCC 14 | libstdc++ 14 | ci20-debug, ci23-release |
| ubuntu-24.04 / x86_64 | Clang 18 | libstdc++ 14, explicitly selected | ci20-debug, ci23-release |
| macos-15 / ARM64 | AppleClang from selected Xcode | libc++ | ci20-debug, ci23-release |
| macos-15 / ARM64 | Homebrew gcc@14 | libstdc++ 14 | ci20-debug, ci23-release |

Eight jobs. There is no macOS Intel row, cross-compile or Rosetta test. The macOS
job checks the native architecture before building. `macos-15` is explicitly
chosen rather than `macos-latest`; however, this label does not freeze Xcode,
Homebrew or OS patch versions. GCC and Clang major versions are checked by CMake;
patch versions and AppleClang's actual version are reported, not silently pinned.

APT and Homebrew install missing tools in their respective CI environments.
An installation failure is an environment/provisioning failure, not evidence that
EasyLocal is incompatible with a compiler. GCC uses the selected macOS SDK
natively; no Apple-specific `-arch` option is forced onto the GNU compiler.
Clang on Linux explicitly selects the GCC 14 installation rather than whichever
libstdc++ a moving image happens to make its default.

All jobs use the same three CMake/CTest operations as local development.
`fail-fast: false` lets every matrix cell finish. Configure, build and test remain
separate steps, and Bash pipeline failures propagate despite `tee`. Reports are
uploaded even after a failed phase when files are available. A failed compile
means the Test step is skipped, not that solver tests have run and failed.

Official actions are pinned to full commit IDs with release comments. The
workflow has read-only repository permissions, disables persistent checkout
credentials, uses `pull_request` rather than `pull_request_target`, and does not
create releases or alter the repository.

## act on the Mac

Prerequisites: a running Docker-compatible container engine and a current act.
With an existing Homebrew setup, `brew install act` installs the CLI; it does not
by itself provide a running container engine. No Docker provider is prescribed.

From the repository root, start with **one** cell:

```sh
act push -W .github/workflows/ci.yml -j linux \
  --matrix toolchain:gcc14 --matrix preset:ci20-debug
```

For Clang, replace `gcc14` with `clang18`. To run all four Linux cells, omit both
`--matrix` filters, retaining `-j linux`.

`.actrc` maps Ubuntu 24.04 to `ghcr.io/catthehacker/ubuntu:act-24.04` and requests
`linux/amd64`, matching the remote Linux architecture. On an ARM64 Mac this is an
emulated Linux x86_64 check; **it is not a macOS Intel check**. The container engine
must support that emulation. Do not change it silently and interpret an ARM Linux
result as the same matrix cell.

act copies the local worktree into the container, so the remote checkout step is
skipped under `ACT`. Artifact upload is also skipped. Provisioning, configure,
build and tests are unchanged. Container output remains in act's console; local
artifact persistence is not configured in this increment. Avoid `--bind` for this
initial workflow. Do not map the macOS job to `-self-hosted`: that would run its
provisioning directly on the development Mac. Native macOS isolation is supplied
by the remote GitHub runner, not by act.

The act container is not a frozen clone of GitHub's VM. A passing local Linux cell
does not guarantee a passing remote cell, and passing under emulation is not a
performance measurement.

## Reports and what to return for review

Each remote cell uploads logs, `toolchain.txt`, `compile_commands.json`, CMake
cache/configuration diagnostics, CTest's last log and `junit.xml`, when produced.
Standard-library macros describe the headers used; they are not a complete
runtime ABI audit. Keep secrets out of CMake cache variables and compiler flags,
since those files are intentionally included in diagnostics.

For the first local run, share the first failing command and its output, or
`build/dev/toolchain.txt` plus `ctest --preset dev --verbose` when it succeeds.
Do not attach a whole build directory. Record verified results only after actual
execution on the Mac or GitHub, referring to the relevant commit and CI run.

## Scope intentionally deferred

The initial matrix is not a full factorial experiment: it does not test C++20
Release or C++23 Debug remotely. Additional compiler majors, Linux libc++, macOS
upstream Clang, Linux ARM64 and sanitizer jobs can be added as separate increments.
The current C++23 cell runs C++20 source in C++23 mode; it does not establish
availability of every C++23 feature. Passing bootstrap tests does not qualify
an eventual framework or a solver application.

## Primary references

Documentation checked while authoring, not execution evidence:

- GitHub runner labels: https://docs.github.com/actions/reference/runners/github-hosted-runners
- Ubuntu image inventory: https://github.com/actions/runner-images/blob/main/images/ubuntu/Ubuntu2404-Readme.md
- macOS ARM64 inventory: https://github.com/actions/runner-images/blob/main/images/macos/macos-15-arm64-Readme.md
- CMake presets: https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html
- CTest WILL_FAIL: https://cmake.org/cmake/help/latest/prop_test/WILL_FAIL.html
- act runners: https://nektosact.com/usage/runners.html
- act matrix filtering: https://nektosact.com/usage/index.html#specifying-matrix
- act image catalogue: https://github.com/catthehacker/docker_images
- Clang toolchain selection: https://clang.llvm.org/docs/ClangCommandLineReference.html
