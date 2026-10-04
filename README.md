<h1>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/branding/easylocal-logo-dark.svg">
    <img alt="EasyLocal" src="docs/assets/branding/easylocal-logo.svg" height="64">
  </picture>
</h1>

[![CI](https://github.com/iolab-uniud/easylocal/actions/workflows/ci.yml/badge.svg)](https://github.com/iolab-uniud/easylocal/actions/workflows/ci.yml)
[![Optional Components](https://github.com/iolab-uniud/easylocal/actions/workflows/optional-components.yml/badge.svg)](https://github.com/iolab-uniud/easylocal/actions/workflows/optional-components.yml)
[![Coverage](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2Fiolab-uniud%2Feasylocal%2Fbadges%2Fcoverage.json)](https://github.com/iolab-uniud/easylocal/actions/workflows/ci.yml)
[![Documentation](https://github.com/iolab-uniud/easylocal/actions/workflows/docs.yml/badge.svg)](https://iolab-uniud.github.io/easylocal/)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

EasyLocal is a **C++23 header-only framework** for local search and
metaheuristics. Version 4 is a complete redesign of EasyLocal++, the
object-oriented framework first described in 2003 (see
[Citing EasyLocal](#citing-easylocal)).

A problem is described by a few components — a solution manager, cost
components, neighborhood explorers and their delta costs — and generic runners
(First and Best Improvement, Hill Climbing, Late Acceptance, Great Deluge,
Simulated Annealing, Tabu Search, and Pareto Late Acceptance for several
objectives) and solvers (LocalSearch, MultiStart, Pipeline) search it. Optional components add an interactive
terminal tester, a REST service and TOML configuration.

New to the library? Start with the [quick start](docs/quick-start.md), then
follow the [tutorial](docs/tutorial/README.md), which builds a TSP solver one
chapter at a time. The [reference](docs/reference/README.md) describes the
contract, API and design choices of each component, and
[API stability](docs/stability.md) what 4.x promises. The documentation is
also published at <https://iolab-uniud.github.io/easylocal/>.

## Requirements

- C++23
- CMake >= 3.25
- Ninja
- GCC or Clang-family compiler

The current CI exercises:

- Linux: GCC 15, GCC 16, Clang 22 and Clang 23, each with libstdc++ and with
  libc++ (Clang 23 from apt.llvm.org)
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

The Python scripts under `scripts/` (documentation snippets, TUI screenshots,
coverage, benchmark summaries) and the TUI end-to-end tests run in a
[uv](https://docs.astral.sh/uv/) environment described by `pyproject.toml` and
`uv.lock`:

```sh
brew install uv
uv sync                                       # creates .venv
uv run scripts/sync-doc-snippets.py           # refresh the docs' code snippets
uv run scripts/tui-snapshots.py build/<preset>/examples/tutorial/easylocal_tutorial_tui
./scripts/coverage.sh                         # coverage of include/easylocal (build/coverage/)
uv run mkdocs serve                           # preview the documentation site
```

Scripts that only use the standard library also run with a plain `python3`,
which is how the test suite runs the snippet check. With the TUI component, the
suite also runs `easylocal.tui-e2e` (label `tui-e2e`): `tests/tui` drives the
tutorial's tester in a pseudo-terminal through `scripts/tui_driver.py` and
checks its user flows on the screen it shows.

## Header-only library

EasyLocal is designed from the beginning as a header-only library.

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

Consumers use the public headers under `include/easylocal/`, organized by
component specialization:

```text
easylocal/
  easylocal.hpp   Core umbrella (everything except adapters/)
  utils/          logging; internal type-level utilities
  config/         typed parameters, parameter sets, CLI/file frontends
  trace/          semantic search events, tracer protocol, recorders
  cost/           cost models (easylocal::cost): value contract and delta,
                  semantic relations, lexicographic and hierarchical costs,
                  cost expressions (sum, in_order, hard_soft, apply)
  helpers/        problem-side components: SolutionManager,
                  NeighborhoodExplorer, neighborhood_union, recipes
  runners/        Runner, search_run, run_control and the search
                  algorithms (easylocal::runners)
  solvers/        orchestration from an Input to a final solution
  testing/        unit-test checks for user components
  app/            app graph, app check, Session
  adapters/       optional components: toml.hpp, tui/, rest/
```

Each directory may include only directories listed above it (`utils/`,
`config/` and `trace/` share the lowest layer; `testing/` sits beside
`runners/`). `cost.hpp`, `helpers.hpp`, `runners.hpp`, `solvers.hpp` and
`trace.hpp` aggregate their directories. The architecture test enforces this
layering and keeps Core free of any adapter dependency. `detail/`
subdirectories are implementation headers, installed but not supported as
direct entry points.

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
package dependency. CI exercises both providers explicitly. TextUI is a second optional component:
`EasyLocal::TUI` uses FTXUI 7.x and is enabled with `EASYLOCAL_ENABLE_TUI=ON`.
`EasyLocal::REST` is a third optional component: it exposes an `app` as a generic
Crow Blueprint and is enabled with `EASYLOCAL_ENABLE_REST=ON`. Core-only
consumers load none of these optional targets or third-party dependencies, even
when the adapters are present in the installation. `ConfigYAML` and `Logging`
remain reserved future integrations. See
[`docs/dependency-policy.md`](docs/dependency-policy.md) for the complete policy
and [`docs/rest.md`](docs/rest.md) for the REST/concurrency model.

The Core/application boundary is intentionally frontend-agnostic.
`<easylocal/easylocal.hpp>` contains only standard-library Core facilities,
including `app`, `check`, and `Session`; ConfigTOML and TextUI remain explicit
adapters. Bound apps and bound runners borrow an lvalue
Input by `const&`; binding a temporary Input is rejected to prevent dangling
references.
The REST adapter uses this same public boundary: HTTP/JSON/Crow/server types stay
outside `EasyLocal::Core`, and each asynchronous run binds fresh mutable
services while sharing only an immutable Input. Cooperative stop/progress
uses the std-only `easylocal::run_control` capability, passed as
`run(solution, ..., easylocal::with(control))`. Every runner is cancellable by
contract: the framework-owned `search_run` checks the control and reports
progress, so adapters can stop any registered runner. TextUI background runs use
the same isolation and control rules.
The Assignment examples include `easylocal_assignment_rest`, which mounts
the generic Blueprint at `/assignment` while leaving Crow server configuration
fully visible to the application. REST run creation uses a stable envelope whose
`input` and optional `initial_solution` members are opaque JSON values interpreted
by the application codec; status/progress, errors, cooperative cancellation,
terminal deletion, and bounded completed-run retention are generic adapter
semantics.

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

The current Assignment, TSP, Exam Timetabling and PFSP MWEs live under
`examples/assignment/`, `examples/tsp/`, `examples/exam_timetabling/` and
`examples/pfsp/` and are intentionally not part of the public include tree.
Exam Timetabling is the reference MWE for multi-component weighted costs and
Simulated Annealing, PFSP for Tabu Search.

Runners and solvers are built with `make_runner<Algorithm>(parameters)` and
`make_solver<Solver>(runner, config)`. Composition has two equivalent
spellings: pipes (`make_runner<A>(p) | sm | nhe`,
`solution_manager<T>() | cost::sum(component<A>(), component<B>())`,
`neighborhood<T>() | delta<C, D>()`) and explicit `with_*` calls
(`.with_solution_manager(sm).with_neighborhood(nhe)`, `.with_cost(expression)`,
`.with_delta<C, D>()`), which document each step. Apps
use the same grammar: `app("name").with_solution_manager(sm)
.with_neighborhood(nhe).with_runner<A>("fi", parameters)`, or
`app("name") | sm | nhe | runner<A>("fi", parameters)`. Assignment
demonstrates the pipes and Exam Timetabling the `with_*` form.

A search algorithm is a class exposing a single `run()` member. The framework
calls it with an `easylocal::search_run`, which gives access to the search
context (neighborhood, evaluation, cost semantics) and owns what every search
shares: evaluation/iteration counters, the evaluation budget, cancellation,
progress reporting and the core trace events. The algorithm only describes its
search logic:

```cpp
class MySearch
{
public:
    using parameters_type = MyParameters; // with a schema: configurable

    explicit MySearch(const MyParameters& parameters);

    template<class Run>
    auto run(Run& run, typename Run::solution_type solution) const
    {
        run.limit_evaluations(parameters_.max_evaluations);
        auto current = run.start(solution);            // run_started

        for (const auto move : run.moves(solution))
        {
            if (run.should_stop())                     // cancelled or budget
            {
                return run.finish(std::move(solution), current.cost());
            }
            auto candidate = run.evaluate_move(solution, current, move);
            // ... run.next_iteration(); run.commit(solution, current, ...);
        }
        return run.finish(
            std::move(solution),
            current.cost(),
            easylocal::termination_reason::local_optimum);
    }
};
```

`finish()` returns an `easylocal::search_result` carrying the solution, cost,
counters and `termination_reason`. The algorithm class is also its own
registration key: `make_runner<MySearch>(MyParameters{...})`,
`app(...).with_runner<MySearch>("name")` and `runner_config<MySearch>()`.
When `MyParameters` has a `parameter_schema()` and `validate()`, the runner
holds the parameters and builds `MySearch` from them when it is bound, so they
are configurable (`search.*`) without any other member.

Callers pass the optional control and tracer as a trailing argument:
`search.run(initial, rng, easylocal::with(control, tracer))`.

The cost always comes from cost components; a SolutionManager only defines
solution semantics. The recipe holds one **cost expression** whose leaves are
the components: a single `component<C>()` is the cost, and several are combined
by `cost::sum` (with weighted terms `component<C>() * w`, weights configurable
as `cost.weights`), `cost::in_order` (lexicographic), `cost::hard_soft`
(hierarchical, the hard components evaluated alone by `two_stage()`) and
`cost::apply` (any function), nested freely. `cost::sum` adds numbers only, so
domain value types never need arithmetic operators just to be summed. TSP
maps its domain value with `cost::apply`, Exam Timetabling uses a weighted
sum, and Assignment a `cost::hard_soft` with a lexicographic hard branch.

Search instrumentation is separate from diagnostic logging.
`<easylocal/trace.hpp>` provides compile-time removable typed search events,
including hierarchical provenance for composite neighborhoods and random-union
selection statistics. An owning memory recorder and post-run JSONL serialization
are provided for trajectory/STN/LON-style analyses. Long runs can instead use an
incremental JSONL recorder that does not retain the event history; see
[`docs/tracing.md`](docs/tracing.md).

All three MWEs now contain runnable `main` programs and load their small problem
instances from versioned files under the corresponding `instances/` directory.
Each `main` owns an application-level `AppParameters` block containing at least
the instance-file path (and an RNG seed for stochastic examples). The runner
then gives its own parameters as a set with relative paths, which the program
adds under a prefix of its choice: `application.*` next to `solver.*`, without
rebuilding the search/neighborhood hierarchy by hand. This keeps application
identity outside the framework types while making the effective runner
configuration introspectable.

The Assignment executable is
`./build/<preset>/examples/assignment/easylocal_assignment`; Exam
Timetabling adds `easylocal_exam_timetabling`; and the TSP executable is
`easylocal_tsp_sa`. The TSP example composes 2-opt and swap neighborhoods
through `neighborhood_union(...)`, applies the bias values held by its
`NeighborhoodUnionParameters<2>` block, attaches child-local tour-length deltas,
and passes an explicit RNG to `run()`. A neighborhood union propagates a
component delta only when every child provides that component; tagged moves are
then dispatched to the originating child's binding without virtual dispatch.

Simulated Annealing is public under `easylocal::runners`. Its hot loop is fully
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
`validate()`. A block may nest another one with
`config::group<"name", &Block::member>`. Defaults remain ordinary member
initializers or constructor values; they are not duplicated in the schema.

`temperature::FixedLengthParameters`, `FirstImprovementParameters`, and
`NeighborhoodUnionParameters<N>` are current framework-side examples. Concrete
parameter blocks live beside the object they configure: search-method parameters
in the search-method header, temperature-policy parameters beside the policy,
and union parameters beside `neighborhood_union`. Each runnable MWE defines its
application-owned `AppParameters` directly in its `main`, because the instance
path and seed belong to the application rather than to EasyLocal. Concrete CLI
and compact configuration-file frontends are layered separately on top of the
source-neutral textual override mapper.

### Parameter sets

Typed blocks are collected in a `config::parameter_set`, which gives each field
a dotted path. Two blocks of the same C++ type occupy distinct paths:

```cpp
easylocal::config::parameter_set parameters;
parameters.add("input", app);
parameters.add("fast.temperature", fast_temperature);
parameters.add("slow.temperature", slow_temperature);
```

`parameters.parameters()` lists every field with its path, description and
value as text. The set stores references to existing blocks; it does not own
or copy configuration values. Field names must be unique within a block at
compile time; a path added twice to a set is rejected with
`std::invalid_argument`.

Framework objects give their parameters as sets with relative paths, and the
caller chooses the prefix:

```cpp
easylocal::config::parameter_set configuration;
configuration.add("application", app);
configuration.add("solver", runner.configuration());
```

`FirstImprovement` contributes `solver.search.*`; Simulated Annealing its policy
as `solver.search.temperature.*`; and a `neighborhood_union(...)`
`solver.neighborhood.random_biases`. The prefix is what distinguishes several
otherwise identical runners (`fast.*`, `slow.*`, and so on).

A runner holds its algorithm's parameters and builds the algorithm, with any
state derived from them (such as a temperature schedule), when it is bound.
Objects that keep derived state themselves, such as a neighborhood union, take
part through `parameters()` and `configure()`: a valid block is committed
through `configure()`. Exposure through a const object is read-only.
Configuration applies to application values and unbound runner
recipes/policies; reconfiguration of an already bound/running search is
deliberately deferred.

### Textual override batches

`parameter_set::apply` (also spelled `easylocal::config::apply_overrides`) is
the shared adapter layer between external text sources and the typed
parameters. A frontend supplies a
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

const auto result = configuration.apply(overrides);
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

`config::cli_help(program, parameters)` derives help directly from the same set
and parameter schemas, including descriptions and current values. CLI syntax
errors are accumulated by the frontend; typed parsing, unknown-path checks,
validation, and transactional commit remain responsibilities of `apply`.

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
`apply` call validates and commits the effective batch. Source syntax
errors and effective typed/validation errors all cause failure with zero commits;
the runnable MWEs return a non-zero exit status after printing diagnostics. CLI
therefore remains a final explicit override layer rather than a second mutation
pass.

## Code style

`.clang-format` and `.clang-tidy` describe the style; `uv sync` installs the
pinned clang-format and clang-tidy.

```sh
git config core.hooksPath .githooks       # once: check formatting on commit
scripts/format.sh                         # format changed lines and examples/
scripts/tidy.sh build/<preset>            # lint the examples
```

The examples are written for people learning the framework: return types come
first (`double evaluate(const Tour& tour) const`), and `auto f()` without a
written type is used only when the type cannot reasonably be spelled, such as a
recipe or an app, with a comment saying so. A control statement whose body is a single
one-line statement takes no braces. They are fully formatted and pass
clang-tidy, which CI checks. The library writes return types first as well
(trailing ones remain only in deduction guides and lambdas), and keeps
`[[nodiscard]]` and `noexcept`; outside `examples/` only the lines a change
touches are formatted, by the hook and in CI, so the code converges as it is
edited rather than in one sweeping reformat.

## Continuous integration

The full CI matrix is intentionally small and targets C++23 directly:

| Platform | Toolchain |
| --- | --- |
| Ubuntu 26.04 | GCC 15 |
| Ubuntu 26.04 | GCC 16 |
| Ubuntu 26.04 | Clang 22 + libstdc++ |
| Ubuntu 26.04 | Clang 22 + libc++ |
| Ubuntu 26.04 | Clang 23 + libstdc++ (apt.llvm.org) |
| Ubuntu 26.04 | Clang 23 + libc++ (apt.llvm.org) |
| macOS ARM64 | AppleClang |
| macOS ARM64 | GCC 16 |
| Windows | clang-cl (Microsoft STL) |

GitHub Actions runs automatically for pull requests and release tags of the form
`vX.Y.Z`, and can also be started manually with `workflow_dispatch`. In addition
to the compiler matrix, the **Optional Components** workflow builds and tests
every optional component (ConfigTOML, TUI, REST) with their dependencies
fetched by CMake FetchContent, on Linux (GCC 16), macOS ARM64 (AppleClang) and
Windows (clang-cl).

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
./scripts/act-ci.sh clang23-libstdcxx
./scripts/act-ci.sh clang23-libcxx
```

## Performance benchmarks

The benchmarks live in
[easylocal-benchmarks](https://github.com/iolab-uniud/easylocal-benchmarks),
which compiles them from an EasyLocal checkout:

- EasyLocal 4 against EasyLocal 3 (`easylocal-legacy` v3.3.1) on the three
  example problems, ported to both frameworks, both measured at every release
  on the same machine;
- the infrastructure: neighborhood traversal (cursors, cursor ranges,
  coroutine ranges), runner-level search and tracing overhead.

Every release tag starts them (the **Benchmarks** workflow sends a
`repository_dispatch`); the results are rendered on the
[Benchmarks](https://iolab-uniud.github.io/easylocal/benchmarks/) page of the
documentation. They are regression diagnostics, with no pass/fail threshold.

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
#include <easylocal/testing.hpp>

const easylocal::testing::fixture<MySolutionManager> f{input, solution};
const auto report = easylocal::testing::check_neighborhood<MyNeighborhood>(f);
```

The same support is available for SolutionManagers, cost components, and delta
evaluators. Each check returns a `check_report`, so it can be integrated with a
test framework or run directly from a small executable:

```cpp
return easylocal::testing::run_checks(
    easylocal::testing::check_solution_manager(f),
    easylocal::testing::check_neighborhood<MyNeighborhood>(f));
```

CTest is the common test entry point locally and in CI:

```sh
ctest --preset dev --output-on-failure
```

Tests are named after stable contracts/responsibilities rather than development
iterations. They evolve with the design and are removed only when the contract
they verify is deliberately abandoned.

## Versioning and releases

`VERSION` is the single source of truth for the EasyLocal version.

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

## Citing EasyLocal

If you use EasyLocal in your research, please cite the 2024 overview paper; the
2003 article describes the original design. `CITATION.cff` carries the same
information for GitHub's "Cite this repository".

```bibtex
@inproceedings{CeschiaDaRosDiGasperoSchaerf2024,
  author    = {Ceschia, Sara and Da Ros, Francesca and Di Gaspero, Luca and Schaerf, Andrea},
  title     = {{EasyLocal++} a 25-year Perspective on Local Search Frameworks: The Evolution of a Tool for the Design of Local Search Algorithm},
  booktitle = {Proceedings of the Genetic and Evolutionary Computation Conference Companion},
  series    = {GECCO '24 Companion},
  pages     = {1658--1667},
  year      = {2024},
  publisher = {Association for Computing Machinery},
  address   = {New York, NY, USA},
  doi       = {10.1145/3638530.3664140},
}

@article{DiGasperoSchaerf2003,
  author  = {Di Gaspero, Luca and Schaerf, Andrea},
  title   = {{EasyLocal++}: An object-oriented framework for flexible design of local search algorithms},
  journal = {Software: Practice and Experience},
  volume  = {33},
  number  = {8},
  pages   = {733--765},
  year    = {2003},
  doi     = {10.1002/spe.524},
}
```

## Authors

EasyLocal is developed at the University of Udine by Sara Ceschia, Francesca
Da Ros, Luca Di Gaspero and Andrea Schaerf.

## License

EasyLocal is released under the [MIT License](LICENSE), as EasyLocal++ was.
The optional components use permissively licensed dependencies: toml++ and
FTXUI (MIT), Crow (BSD-3-Clause) and Asio (Boost Software License 1.0).

## Legacy reference

The legacy reference is Bitbucket `satt/easylocal-3`, branch `no_output`,
reviewed at commit `b40b14c2db2bdc81574a0613c52674644f8a0101`.

The unfinished redesign in
[iolab-uniud/easylocal-legacy](https://github.com/iolab-uniud/easylocal-legacy)
is **not** the architectural baseline.
