# Working on EasyLocal

Conventions for coding agents (and people) changing this repository. The
documentation in `docs/` explains the library; this file explains how the
repository is maintained.

## Layout

- `include/easylocal/` is the header-only library. Its directories are layers,
  and a header includes only headers of its own layer or a lower one:
  `utils`, `config`, `trace` < `cost` < `helpers` < `runners`, `testing` <
  `solvers` < `app` < `adapters`. The test `easylocal.architecture-boundary`
  enforces it, and core headers never include an adapter (TUI, REST, TOML).
- `examples/` are complete programs read by students: keep them simple.
  `examples/quickstart` and `examples/tutorial` are the code of the quick
  start and of the tutorial, which take their snippets from them;
  `examples/tsp` models the same TSP as a complete example under its own
  names (only the tutorial's irace section quotes it), and `assignment`,
  `exam_timetabling` and `pfsp` are the other problems.
- `tests/` are the test programs, registered in `tests/CMakeLists.txt`.
- `docs/` is the documentation site (MkDocs): tutorial, reference, and
  `docs/roadmap.md` for planned evolutions not yet scheduled.

## Building and testing

```sh
cmake --preset dev && cmake --build build/dev && ctest --test-dir build/dev -j8
```

The local check before a commit is meant to find problems quickly; CI is the
comprehensive one. Run, the fastest first:

- `scripts/format.sh --check`;
- `ctest -j8` on `build/dev` (the platform's compiler, AppleClang with libc++
  on macOS);
- when `include/` or `tests/` change, `ctest -j8` on a GCC 15 Release build:
  the oldest GCC, with libstdc++ and the `-O3` warnings; other GCC and Clang
  versions are left to CI. CI's Linux job builds it with the `ci23-release`
  preset and `CXX=g++-15`; locally, `cmake --preset release -B build/gcc15
  -DCMAKE_CXX_COMPILER=g++-15 && cmake --build build/gcc15 && ctest
  --test-dir build/gcc15 -j8` (on macOS the Homebrew `g++-15`);
- `scripts/tidy.sh build/dev` when `examples/` change, or the library they use;
- `uv run mkdocs build --strict` when `docs/` changes.

An optional component (TOML, TextUI, REST, irace) is checked locally only when
the change touches it: build that component's targets in a build that enables
it and run its tests (`ctest -L tui-e2e`, `-L rest-http`, `-L irace`, or
`-R <name>`). The build with every optional component and the API reference
(MrDocs) are left to CI, as are the other compilers.

Never run `scripts/build-and-test.sh --exhaustive` (every feature subset): it
is a last resort. CI is the next check: push and dispatch the GitHub
workflows, which do not run on pushes to `main`:

```sh
gh workflow run ci.yml --ref main
gh workflow run optional-components.yml --ref main
```

A dispatch, like a pull request, is a quick run: the local check (format and
lint, AppleClang, GCC 15 Release, the optional components on Linux) and
Windows clang-cl. `-f level=full` runs everything, as a release tag does:
Linux GCC 15 and 16, Clang 22 and 23 with libstdc++ and with libc++, macOS
AppleClang and GCC 16, the optional components on the three systems, a GCC
16 Debug build with AddressSanitizer and UBSan (the `asan` preset, also usable
locally) and coverage. Every job builds with warnings as errors. Code that
builds with one compiler may not build with another: when CI fails, reproduce
it with that compiler locally (for example a GCC 15 Release build), or run a
Linux job in Docker with `scripts/act-ci.sh <toolchain>`.

Python tools (`scripts/`, the documentation, the TUI end-to-end tests) run in
the uv environment: `uv sync`, then `uv run ...`; `uv run mkdocs serve`
previews the site, `scripts/coverage.sh` measures the coverage of
`include/easylocal` (the CI badge).

## Code style

- Return types come first, in the library as in the examples: `double
  evaluate(...) const`. Trailing return types only in deduction guides and
  lambdas. `auto f()` only where the type cannot be spelled.
- The library keeps `[[nodiscard]]` and `noexcept`; the examples leave them out
  of user hooks, and do not put `const` on by-value parameters.
- A neighborhood explorer's `moves()` is an `easylocal::generator<Move>` written
  with `co_yield`, never a filled vector.
- In the examples, do not write a delta cost component that scans the whole
  solution: let the framework evaluate the candidate.
- Write `EASYLOCAL_NO_UNIQUE_ADDRESS` (`easylocal/utils/detail/attributes.hpp`),
  never the raw `[[no_unique_address]]`, which MSVC and clang-cl ignore; the
  test `easylocal.portable-attributes` rejects it.
- The library calls `(std::min)(a, b)`, `(std::max)(a, b)` and
  `(std::numeric_limits<T>::max)()`, in parentheses, which the `min` and `max`
  macros of `windows.h` do not expand; the test `easylocal.windows-macros`
  compiles the core headers with them defined.
- The library spells the types of a `std::tuple`, `std::tuple<T>{x}`, never
  `std::tuple{x}`: with one argument, or a pack that may hold one, some
  compilers find the deduction ambiguous. The test `easylocal.tuple-types`
  rejects it.
- Variants on a hot path (an inverse, a move evaluation) are chosen at compile
  time, with templates, rather than by runtime parameters.
- A member with a const and a mutable version of the same body is written once,
  with an explicit object parameter (`template<class Self> auto& f(this Self&&
  self)`, which also takes a temporary, as the two overloads did), not as two
  overloads.
- Names: PascalCase for the types users name in their code, the classes they
  choose and hold: the algorithms and policies (`SimulatedAnnealing`,
  `LocalSearch`, `Cyclic`), their parameter blocks
  (`SimulatedAnnealingParameters`), the objects that run them (`Runner`,
  `BoundRunner`, `App`, `BoundApp`, `Session`, `Pipeline`). snake_case for the
  vocabulary and the infrastructure: functions and factories (`app()`,
  `make_runner()`, `runner<A>()`), the cost vocabulary (`hierarchical`,
  `limit`), results and events (`search_result`, `run_effort`), parameters and
  tracing types (`parameter_set`, `memory_recorder`), and every name in
  `detail`. What a factory returns to be added to something else (a recipe, a
  registration) is infrastructure: its brief says "registration" or "recipe".
- Prefer `struct` for transparent value types; prefer `class` for
  encapsulated abstractions. A type with an invariant (fields that must stay
  consistent, state changed only through its members) is a `class`, with its
  fields private. A type whose fields may each be set freely is a `struct`:
  parameter blocks (aggregates, built with designated initializers),
  descriptors, traits, tags and stateless function objects, even when they
  have member functions.
- Comments match the surrounding code: a short description of each class or
  function, no narration of the change.

## API comments

The generated API reference (`scripts/api-docs.py`, MrDocs) is made of the
`///` comments of `include/`.

- The comment of a public declaration (a class, a concept, a function, an
  alias, a public field) and the leading comment of a header (`/// \file`) are
  written with `///`; every other comment (in `detail`, in a body, on a private
  member) with `//`.
- The first paragraph is the brief, shown in the indexes: one sentence, then a
  `///` line before the rest. Wrap the text at 80 columns.
- A class says what it does and how it ends or what it returns; a parameter
  block is "The parameters of X.", and each field says what it bounds or sets
  (its unit, what its default or `unlimited` means).
- A template constrained by concepts of `detail` says what the user must
  provide, in user terms, as the last sentence: "Requires a neighborhood
  explorer with random_move() and a cost with better()." The `detail` concepts
  are not in the reference, so their names alone tell the reader nothing.
- An algorithm's `run(run, solution, ...)` says that the bound runner calls it;
  a member of a policy interface is described once, on its concept.
- Code and paths with angle brackets go in backticks (`` `runners.<name>.*` ``),
  or MrDocs reads them as HTML; a code example is a paragraph indented by four
  spaces.
- Every public declaration has a comment: types, concepts, enums and their
  values, aliases, functions, members and public fields, the members of a
  class template specialization, special members (constructors, assignments,
  destructors) and the overloads of a function each. One without a comment
  fails the API build, in CI as with `uv run scripts/api-docs.py
  build/<preset>`; `--undocumented [--only <dir>/]` lists them. Parameters and
  return values are explained in the text, not with their own commands.
- A deleted member says why, each one of a group: "Not copyable or movable:
  its services refer to each other."; "Deleted: the BoundRunner borrows the
  Input, which a temporary would leave dangling."
- Repeated members have the same comment everywhere: `parameter_schema()`,
  "The names, members and descriptions of the parameters."; `validate()`,
  "Whether the parameters are valid, and why not."; `parameters_type`, "The
  parameter block of the algorithm." (or of the policy, the list...);
  `parameters()`, "The parameters."; a constructor from them, "From its
  parameters.".

## Formatting

`.clang-format` (pinned in `pyproject.toml`) formats `examples/` fully and the
rest of the repository only on the lines a change touches: do not reformat a
whole library or test file for its own sake. `scripts/format.sh` does this,
and the pre-commit hook (`git config core.hooksPath .githooks`) checks the
staged lines; `uv run git-clang-format --staged` fixes them. The exception is a
file the hook sees as new, such as one renamed with `git mv`: all its lines
are staged, so it is formatted whole, in the same commit.

## Documentation

- The code snippets of the tutorial are generated from `examples/quickstart` and
  `examples/tutorial`: change the example, then run
  `scripts/sync-doc-snippets.py` (the test `easylocal.docs.snippets` checks
  they agree). Edit the prose in `docs/` directly.
- Where a page shows code of both versions, as `docs/from-easylocal-3.md`
  does, each block carries a caption: ```` ```cpp title="EasyLocal 3" ```` or
  ```` ```cpp title="EasyLocal 4" ````.
- When the TextUI changes, regenerate its screenshots with
  `uv run scripts/tui-snapshots.py build/<preset>/examples/tutorial/easylocal_tutorial_tui`.
- Every user-visible change goes in `CHANGELOG.md`, and in the reference page
  of its component.
- Plans go in `docs/roadmap.md` (why, what, when), not in the code.

## Commits

- Several sessions may work in the same checkout. Stage only the files you
  changed, by path (never `git add -A` or `git add -u`), and check the diff of
  shared files (`CHANGELOG.md`, `tests/CMakeLists.txt`, CMake files) hunk by
  hunk: leave others' changes out.
- Messages follow Conventional Commits (`feat(runners): ...`, `fix(rest): ...`,
  `docs: ...`, `ci: ...`), with `!` for a breaking change.
- Until 4.0.0 an API may change without keeping backward compatibility;
  `docs/stability.md` says what each release promises.

## Releases

`VERSION` holds the version in preparation (such as `4.0.0-alpha.2`), whose section in
`CHANGELOG.md` reads "not yet released". `scripts/release.sh` releases it,
from `main` up to date with `origin/main`: it
dates the section and `CITATION.cff`, runs the Release build and its tests,
commits, creates the annotated tag `vVERSION`, pushes both and creates the
GitHub release from the section (`--page-only` creates it alone, for an
existing tag). The tag runs the full CI and the benchmarks. Then `VERSION` gets the next version, with its section in
`CHANGELOG.md`.
