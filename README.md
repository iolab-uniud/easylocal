# EasyLocal++

[![CI](https://github.com/iolab-uniud/easylocal-next/actions/workflows/ci.yml/badge.svg)](https://github.com/iolab-uniud/easylocal-next/actions/workflows/ci.yml)

EasyLocal++ is an incremental redesign of EasyLocal as a modern **C++23
header-only library** for local search and metaheuristics.

The project is being rebuilt from concrete minimal working examples, with tests
and the public API evolving incrementally from the contracts they expose.

> **Current status:** the public recipe-based `Runner`, deterministic n-ary
> neighborhood union, cursor-to-range neighborhood adapter, aggregation
> facilities, single random proposals, First/Best Improvement, and Simulated
> Annealing are in place. Domain models remain example-local while additional
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

The canonical public CMake target is:

```cmake
EasyLocal::Core
```

`EasyLocal::Core` is intentionally dependency-free beyond the C++ standard
library. `EasyLocal::EasyLocal` remains available as a compatibility facade and
links only to `EasyLocal::Core`.

A configured build tree can be installed to any prefix:

```sh
cmake --install build/release --prefix /path/to/easylocal-prefix
```

An external CMake project can then consume that installation with:

```cmake
find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core)

add_executable(my_search main.cpp)
target_link_libraries(my_search PRIVATE EasyLocal::Core)
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
with both legacy `find_package(EasyLocal)` discovery and explicit
`COMPONENTS Core`, builds it through `EasyLocal::Core`, and runs the resulting
executable. The package test also verifies that `EasyLocal::Core` exposes no
third-party link dependency and that requesting an unavailable optional
component fails diagnostically.

### Optional dependency policy

The framework core and the std-only CLI/compact configuration frontends do not
require external libraries. TOML support is the first concrete optional adapter:
`EasyLocal::ConfigTOML` uses `toml++` and is enabled with
`EASYLOCAL_ENABLE_CONFIG_TOML=ON`. It is deliberately absent from Core-only
installations and from `<easylocal/easylocal.hpp>`. Consumers request it
explicitly:

```cmake
find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core ConfigTOML)
target_link_libraries(my_solver PRIVATE EasyLocal::Core EasyLocal::ConfigTOML)
```

The adapter first uses `find_package(tomlplusplus 3.4 CONFIG)`. A pinned
`FetchContent` fallback (`v3.4.0`) is permitted only when the top-level caller
explicitly sets `EASYLOCAL_FETCH_DEPENDENCIES=ON` (default `OFF`), so configuring
EasyLocal never performs implicit network access. When toml++ comes from
FetchContent, EasyLocal installs a private copy of its headers under the
EasyLocal include tree so an installed `ConfigTOML` component remains
self-contained and relocatable. A system-provided toml++ remains an external
package dependency. CI exercises both providers explicitly. `ConfigYAML`, `Logging`, and `TUI` remain reserved
optional component names for later integrations. See
[`docs/dependency-policy.md`](docs/dependency-policy.md) for the complete policy.

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

The current Assignment, TSP, and Exam Timetabling MWEs live under
`examples/assignment/`, `examples/tsp/`, and `examples/exam_timetabling/` and are
intentionally not part of the public include tree. Exam Timetabling is the
reference MWE for multi-component weighted costs and Simulated Annealing.

Cost aggregation is explicit whenever domain semantics require it, but simple
weighted costs have a convenience default. If all active component values can
be combined as a weighted sum, omitting `aggregator(...)` materializes a
configurable unit-weight `aggregation::weighted_sum` and emits a runtime warning.
The weights remain exposed as `cost.weights`; if no safe weighted expression can
be inferred, an explicit aggregator is still required. TSP demonstrates the
implicit spelling, Exam Timetabling keeps the explicit built-in weighted sum,
and Assignment uses a custom hierarchical aggregator.

All three MWEs now contain runnable `main` programs and load their small problem
instances from versioned files under the corresponding `instances/` directory.
Each `main` owns an application-level `AppParameters` block containing at least
the instance-file path (and an RNG seed for stochastic examples). The runner
then exposes its own read-only configuration subtree, so the application root
combines `application.*` with a caller-named runner node such as `solver.*`
without manually rebuilding the search/neighborhood hierarchy. This keeps
application identity outside the framework types while making the effective
runner configuration introspectable.

The Assignment executable is
`./build/<preset>/examples/assignment/easylocal_assignment_mwe`; Exam
Timetabling adds `easylocal_exam_timetabling_mwe`; and the TSP executable is
`easylocal_tsp_sa_mwe`. The TSP example composes 2-opt and swap neighborhoods
through `neighborhood_union(...)`, applies the bias values held by its
`NeighborhoodUnionParameters<2>` block, attaches child-local tour-length deltas,
and passes an explicit RNG to `run()`. A neighborhood union propagates a
component delta only when every child provides that component; tagged moves are
then dispatched to the originating child's binding without virtual dispatch.

Simulated Annealing is public under `easylocal::search`. Its hot loop is fully
policy based and uses no virtual dispatch: the concrete temperature and
acceptance policy types are template parameters, while their configuration and
run state remain ordinary runtime data stored by value. The standard
`MetropolisAcceptance` requires an arithmetic, non-`bool` cost; for this S20
contract that numeric cost is directly the SA energy, with no Cost-to-Energy
adapter.

## Typed parameter leaves

The first configuration layer is intentionally limited to typed parameter
blocks. A parameter block owns its ordinary C++ values, declares an internal
`parameter_schema()` next to those values, and validates its own invariants with
`validate()`. Generic code can inspect the declared fields through
`easylocal::config::for_each_parameter` without type erasure or a dynamic
registry. Defaults remain ordinary member initializers or constructor values;
they are not duplicated in the schema.

`temperature::FixedLengthParameters`, `FirstImprovementParameters`, and
`NeighborhoodUnionParameters<N>` are current framework-side examples. Concrete
parameter blocks live beside the object they configure: search-method parameters
in the search-method header, temperature-policy parameters beside the policy,
and union parameters beside `neighborhood_union`. Each runnable MWE defines its
application-owned `AppParameters` directly in its `main`, because the instance
path and seed belong to the application rather than to EasyLocal. Concrete CLI
and compact configuration-file frontends are layered separately on top of the
source-neutral textual override mapper.

### Named configuration tree

Typed leaves can be assembled into a non-owning configuration tree
without flattening instance identity into the parameter type. For example, two
`FixedLengthParameters` blocks of the same C++ type can occupy distinct paths:

```cpp
auto tree = easylocal::config::root(
    easylocal::config::named<"input">(app),
    easylocal::config::named<"fast">(
        easylocal::config::named<"temperature">(fast_temperature)),
    easylocal::config::named<"slow">(
        easylocal::config::named<"temperature">(slow_temperature)));
```

`config::for_each_config_parameter` traverses the tree read-only and exposes a compile-time
segmented `parameter_path`, the original field descriptor, and a typed const
reference to the value. The tree stores references to existing parameter blocks;
it does not own or copy configuration values. Sibling node names and field names
within a parameter block must be unique at compile time. A named node may expose
both local parameters and child nodes, which is required for compositional
objects such as a neighborhood union that owns selection parameters and may also
contain configurable children.

Framework objects can expose this structure directly. A fully configured
`Runner` offers a caller-named subtree:

```cpp
auto configuration = easylocal::config::root(
    easylocal::config::named<"application">(app),
    runner.configuration<"solver">());
```

`FirstImprovement` contributes `solver.search.*`; Simulated Annealing exposes
its configurable policy hierarchy such as `solver.search.temperature.*`; and a
`neighborhood_union(...)` contributes `solver.neighborhood.random_biases`.
The caller-supplied runner name is what distinguishes multiple otherwise
identical runner instances (`fast.*`, `slow.*`, and so on).

Mutable framework/application objects additionally expose validated endpoints.
`config::at<...>(tree)` resolves one node by compile-time path; callers take a
typed snapshot, modify it, and commit the whole block through `configure()`:

```cpp
const auto configuration = easylocal::config::root(
    easylocal::config::named<"application">(app),
    runner.configuration<"solver">());

const auto& temperature =
    easylocal::config::at<"solver", "search", "temperature">(
        configuration);

auto parameters = temperature.parameters();
parameters.max_iterations = 500;

const auto validation = temperature.configure(parameters);
```

Validation occurs before the owned block is replaced. Policy endpoints can also
rebuild derived configuration state before committing; for example,
`FixedLength` recomputes its temperature-level schedule. A failed update leaves
the previous configuration unchanged. Exposure through a const object remains
read-only. Configuration currently applies to application values and unbound
runner recipes/policies; reconfiguration of an already bound/running search is
deliberately deferred. Concrete external frontends are layered on top of the
same transactional textual-override adapter rather than changing runtime search
objects directly.

### Textual override batches

`easylocal::config::apply_overrides` is the shared adapter layer between
external text sources and the typed configuration tree. A frontend supplies a
batch of dotted paths plus textual values; the mapper resolves the paths,
parses each value using the field's C++ type, stages all affected parameter
blocks, validates them, and commits only when the complete batch is valid.

```cpp
constexpr std::array overrides{
    easylocal::config::text_override{
        "application.instance_file", "instances/sample.tsp"},
    easylocal::config::text_override{
        "solver.search.temperature.max_iterations", "500"},
    easylocal::config::text_override{
        "solver.neighborhood.random_biases", "[3, 1]"},
};

const auto result = easylocal::config::apply_overrides(
    configuration, overrides);
```

The initial built-in textual types are `bool`, integral and floating-point
values, `std::string`, `std::filesystem::path`, and fixed `std::array` values
whose elements are themselves supported. Fixed arrays accept bracketed or
comma-separated forms such as `[3, 1]` and `3,1`. Parsing and validation
diagnostics are accumulated rather than stopping at the first error. The
result distinguishes duplicate paths, unknown parameters, read-only targets,
parse failures, and block-validation failures; diagnostics retain the offending
path and textual value when one exists.

A batch is globally transactional: if any diagnostic is produced, no parameter
block is modified. Cross-field changes to the same block are staged together
and validated once, so overrides can move directly between two valid states
without requiring every intermediate field assignment to be valid.

### Command-line frontend

`easylocal::config::parse_cli` accepts explicit long options that mirror
configuration paths, either as `--path value` or `--path=value`, plus
`-h`/`--help` and the reserved `--config <file>` frontend option. Every
configuration override carries a textual value; there are deliberately no
implicit boolean flags or abbreviations, so the frontend remains a thin
projection onto `text_override`.

```sh
./solver \
  --application.instance_file instances/sample.tsp \
  --application.seed=2026 \
  --solver.search.temperature.cooling_rate=0.8 \
  --solver.neighborhood.random_biases='[3, 1]'
```

`config::cli_help(program, tree)` derives help directly from the same tree and
parameter schemas, including descriptions and current values. CLI syntax errors
are accumulated by the frontend; typed parsing, unknown-path checks, validation,
and transactional commit remain responsibilities of `apply_overrides`.

### Compact configuration-file frontend

`easylocal::config::load_config_file` reads the intentionally small line-based
format used by the runnable MWEs:

```text
# Full-line comments begin with #.
application.seed = 2026
solver.search.temperature.cooling_rate = 0.8
solver.neighborhood.random_biases = [3, 1]
```

Each non-comment line is exactly `path = textual-value`; the first `=` separates
the structural path from the value, so the right-hand side is passed to the same
S27a typed parser used by the CLI. The file frontend reports open errors, malformed
lines, empty paths, duplicate paths, and source line numbers without modifying
configuration.

The effective precedence is:

```text
C++ construction/defaults < configuration file < CLI
```

`overlay_overrides` resolves file-vs-CLI precedence first, then one single
`apply_overrides` call validates and commits the effective batch. Source syntax
errors and effective typed/validation errors all cause failure with zero commits;
the runnable MWEs return a non-zero exit status after printing diagnostics. CLI
therefore remains a final explicit override layer rather than a second mutation
pass.

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
- single random proposals through `random_move(solution, rng)` and deterministic
  seeded proposal behavior;
- n-ary neighborhood-union random proposals, including explicit child-selection
  biases;
- per-component delta propagation through neighborhood unions, including nested
  unions and full-evaluation fallback when a child lacks a component delta;
- public First/Best Improvement and Simulated Annealing integration through the Runner.

EasyLocal also provides framework-agnostic checks for user-defined services:

```cpp
#include <easylocal/testing/checks.hpp>

const auto report = easylocal::testing::check_neighborhood<MyNeighborhoodCheck>();
```

The same support is available for SolutionManagers, cost components, and delta
evaluators. Each check returns a `check_report`, so it can be integrated with a
test framework or run directly from a small executable:

```cpp
return easylocal::testing::run_checks(
    easylocal::testing::check_solution_manager<MySMCheck>(),
    easylocal::testing::check_neighborhood<MyNeighborhoodCheck>());
```

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
