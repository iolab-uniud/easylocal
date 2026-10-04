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

Before committing, run:

- `ctest -j8` on `build/dev` and on a build with every optional component
  (configure with `-DEASYLOCAL_ENABLE_CONFIG_TOML=ON -DEASYLOCAL_ENABLE_TUI=ON
  -DEASYLOCAL_ENABLE_REST=ON -DEASYLOCAL_FETCH_DEPENDENCIES=ON`);
- `scripts/format.sh --check` and `scripts/tidy.sh build/dev`;
- `uv run mkdocs build --strict` when `docs/` changes, and
  `uv run scripts/api-docs.py build/<preset>` (MrDocs, warnings as errors) when
  the `///` comments change.

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
- Comments match the surrounding code: a short description of each class or
  function, no narration of the change.
- In `include/`, the comment of a public declaration (a class, a function, an
  alias, a public field) and the leading comment of a header (`/// \file`) are
  written with `///`, which MrDocs reads for the generated API reference;
  every other comment (in `detail`, in a body, on a private member) with `//`.

## Formatting

`.clang-format` (pinned in `pyproject.toml`) formats `examples/` fully and the
rest of the repository only on the lines a change touches: never reformat a
whole library or test file. `scripts/format.sh` does this, and the pre-commit
hook (`git config core.hooksPath .githooks`) checks the staged lines;
`uv run git-clang-format --staged` fixes them.

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
