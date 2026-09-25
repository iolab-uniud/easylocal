# External dependency policy

EasyLocal++ keeps its algorithmic and configuration core independent from
third-party libraries. External packages may be used only by optional adapter
targets whose dependencies are visible to consumers through those targets.

## Public target model

`EasyLocal::Core` is the canonical public target. It is header-only, requires
C++23, and has no third-party link dependencies. Existing code may continue to
use `EasyLocal::EasyLocal`; that target is a compatibility facade that links
only to `EasyLocal::Core`.

Future optional integrations use separate package components and targets. The
reserved naming scheme is:

| Package component | Target | Purpose |
| --- | --- | --- |
| `Core` | `EasyLocal::Core` | dependency-free framework and std-only frontends |
| `ConfigTOML` | `EasyLocal::ConfigTOML` | TOML configuration adapter |
| `ConfigYAML` | `EasyLocal::ConfigYAML` | YAML configuration adapter |
| `Logging` | `EasyLocal::Logging` | external logging integration |
| `TUI` | `EasyLocal::TUI` | optional textual UI integration |

The optional targets above are names reserved by the policy; S28a does not yet
select or require concrete third-party libraries for them.

## Dependency resolution

An optional adapter follows this order when it is enabled:

1. try to use an already installed package through `find_package`;
2. if the package is unavailable and `EASYLOCAL_FETCH_DEPENDENCIES=ON`, the
   adapter may use an explicit `FetchContent` fallback;
3. otherwise configuration fails with a diagnostic naming the missing package
   and the EasyLocal feature that requested it.

`EASYLOCAL_FETCH_DEPENDENCIES` defaults to `OFF`. Configuring EasyLocal must
never initiate network access unless the caller explicitly enables that option.
Vendoring third-party source into the EasyLocal repository is not the default
strategy and requires a separate design decision.

Future feature switches use `EASYLOCAL_ENABLE_<FEATURE>` names and should be
`OFF` by default when the feature introduces a third-party dependency.

## Installation and consumers

The installed package supports component-aware discovery. The dependency-free
core can be requested explicitly:

```cmake
find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core)
target_link_libraries(my_solver PRIVATE EasyLocal::Core)
```

Legacy consumers remain valid:

```cmake
find_package(EasyLocal CONFIG REQUIRED)
target_link_libraries(my_solver PRIVATE EasyLocal::EasyLocal)
```

When optional adapters are introduced, only adapters built and installed by the
producer may report their package component as available. Requesting an
unavailable component must fail during `find_package`; it must not silently fall
back to the core or download dependencies in the consumer project.

## Architectural boundary

Third-party types must not leak into the core Runner, neighborhood, search,
parameter, or configuration-tree contracts. An adapter translates between its
external library and a stable EasyLocal-facing abstraction. In particular:

- TOML/YAML adapters produce the same textual overrides used by the std-only
  configuration frontends;
- a logging integration must not make the core depend on the chosen logging
  library or formatting library;
- a TUI consumes application/configuration/search-observation facilities rather
  than becoming part of the Runner itself.

This keeps `#include <easylocal/easylocal.hpp>` and `EasyLocal::Core` usable in a
pure standard-library consumer regardless of which optional integrations exist
in the source tree.
