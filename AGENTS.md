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
- when `include/` or `tests/` change, a GCC 16 build with its tests and a
  GCC 15 Release build (`-O3` warnings): GCC covers libstdc++, so a local Clang
  with libstdc++ build is not needed;
- `scripts/tidy.sh build/dev` when `examples/` change, or the library they use;
- `uv run mkdocs build --strict` when `docs/` changes.

An optional component (TOML, TextUI, REST, irace) is checked locally only when
the change touches it: build that component's targets in a build that enables
it and run its tests (`ctest -L tui-e2e`, `-L rest-http`, `-L irace`, or
`-R <name>`). The build with every optional component and the API reference
(MrDocs) are left to CI, as are the other compilers.

Never run `scripts/build-and-test.sh --exhaustive` (every feature subset): it
is a last resort. For
a comprehensive check push and dispatch the GitHub workflows, which do not run
on pushes to `main`:

```sh
gh workflow run ci.yml --ref main
gh workflow run optional-components.yml --ref main
```

CI covers Linux (GCC 15 and 16, Clang 22 and 23 with libstdc++ and libc++),
macOS ARM64 (AppleClang, GCC 16) and Windows (clang-cl), with warnings as
errors. Code that builds with one compiler may not build with another: when CI
fails, reproduce it with that compiler locally (for example a GCC 15 Release
build).

Python tools (`scripts/`, the documentation, the TUI end-to-end tests) run in
the uv environment: `uv sync`, then `uv run ...`.

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
  values, aliases, functions, members and public fields. One without a comment
  fails the API build, in CI as with `uv run scripts/api-docs.py
  build/<preset>`; `--undocumented [--only <dir>/]` lists them. Parameters and return values are explained in the
  text, not with their own commands.
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
- EasyLocal 4 has no release yet: an API may change without keeping backward
  compatibility.
