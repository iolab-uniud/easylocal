# API stability

EasyLocal follows [Semantic Versioning](https://semver.org/). This page states
what the version number promises in the 4.x series.

## Pre-releases

The versions `4.0.0-alpha.N` and `4.0.0-beta.N` promise nothing: any part of
the library, header paths included, may change between one pre-release and the
next, and every incompatible change is noted in the
[changelog](https://github.com/iolab-uniud/easylocal/blob/main/CHANGELOG.md).
The levels below apply from 4.0.0.

The installed CMake package reports the numeric version in `EasyLocal_VERSION`
(`find_package(EasyLocal 4.0)` accepts a pre-release of 4.0.0) and the
pre-release in `EasyLocal_VERSION_PRERELEASE` (`alpha.1`, empty for a
release).

## Levels

| Level | What | Promise |
| --- | --- | --- |
| **Stable** | The headers under `include/easylocal/` outside `detail`: the problem model (`helpers/`: concepts, bases, neighborhood union, recipes), `cost/`, `Runner`, `BoundRunner`, `make_runner` / `make_solver`, the pipe and `with_*` composition, the built-in runners and their `parameters_type`, `search_result`, `termination_reason`, `search_result_for`, `run_control` and the run options (`with`, `stop_at`, `timeout`, `max_evaluations`), the solvers and their configurations, `app`, `App` and `BoundApp` (their members keyed by name), `check`, `Session`, `cli::run` and its switches, the I/O hooks of `app/io.hpp` (`load_input`, `load_solution`, `save_solution`, `describe`), `config/` (parameters, command line, configuration files), `testing/`. The CMake targets `EasyLocal::*` and the `EASYLOCAL_*` options. | No incompatible change before 5.0. |
| **Stable extension API** | `search_run`, for writing runners: the members documented in [chapter 7](tutorial/07-custom-runner.md) and in the [runners reference](reference/runners.md#search_run). | Members may be added in minor releases, none removed or changed. |
| **Experimental** | The optional adapters' surface: TextUI options and layout, REST routes and JSON envelopes; the trace event schema and the JSONL and binary trace formats; the export to irace (`app/tuning.hpp`: `--tuning.irace`, `--tuning.print`, `TuningParameters`) and the files it writes; logging (`utils/logging.hpp`), which the library does not use yet. | May change in a minor release, always noted in the [changelog](https://github.com/iolab-uniud/easylocal/blob/main/CHANGELOG.md). |
| **Internal** | `detail` namespaces, `*/detail/` headers, `EASYLOCAL_DETAIL_*` macros, anything not documented. | None. |

Adding members to stable types, overloads, concepts' optional capabilities,
enumerators to `termination_reason` and new runners, solvers or cost models are
compatible changes. Code that switches exhaustively over `termination_reason`
should keep a default branch.

## Reproducibility

A seed reproduces a run or a solve on the same platform and standard library,
under these conditions:

- **No time limit.** A run bounded by evaluations, iterations or a target is
  reproduced; one ended by a time limit (`timeout`, `--timeout`, the TextUI's
  *seconds*, REST's `timeout`) stops after however many evaluations the
  machine makes in that time.
- **The same commands, in the same order.** A session (`Session`,
  `cli::run`, the TextUI, each REST run) seeds its RNG once. A random solution
  draws from it, and each run gets a generator of its own, seeded with one
  draw of it: the same seed and the same sequence of commands (random
  solutions, runs, random moves) give the same runs in every frontend, and
  the second run of a session differs from the first.
- **A solver's stream.** A solver (`LocalSearch`, `MultiStart`, a pipeline)
  seeds its RNG once, at construction or with `.seed(...)`; each `solve()`
  continues the stream, so its seed reproduces the sequence of solves, not
  each solve on its own.
- **REST seeds.** A REST run is reproduced by its seed: the request's `seed`,
  or `blueprint_options::seed` plus the run id, which depends on the runs the
  service started before it.
- **The same standard library.** Results are not reproducible across standard
  libraries (libstdc++, libc++, Microsoft's): `std` distributions such as
  `std::uniform_int_distribution`, `std::shuffle` and `std::hash` (which the
  helpers of `utils/hash.hpp` combine) are implementation-defined.

## Supported toolchains

The compilers and platforms of the CI matrix are supported (see the
[changelog](https://github.com/iolab-uniud/easylocal/blob/main/CHANGELOG.md)
and the README). Dropping one happens in a minor release and is noted in the
changelog.
