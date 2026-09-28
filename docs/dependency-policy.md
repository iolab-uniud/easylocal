# External dependency policy

EasyLocal++ keeps its algorithmic and configuration core independent from
third-party libraries. External packages may be used only by optional adapter
targets whose dependencies are visible to consumers through those targets.

## Public target model

`EasyLocal::Core` is the canonical public target. It is header-only, requires
C++23, and has no third-party link dependencies. Existing code may continue to
use `EasyLocal::EasyLocal`; that target is a compatibility facade that links
only to `EasyLocal::Core`.

Optional integrations use separate package components and targets. The
component model is intentionally one-way: adapters depend on Core, never the
reverse. Current and reserved names are:

| Package component | Target | Purpose |
| --- | --- | --- |
| `Core` | `EasyLocal::Core` | dependency-free framework and std-only frontends |
| `ConfigTOML` | `EasyLocal::ConfigTOML` | TOML configuration adapter |
| `ConfigYAML` | `EasyLocal::ConfigYAML` | YAML configuration adapter |
| `Logging` | `EasyLocal::Logging` | external logging integration |
| `TUI` | `EasyLocal::TUI` | FTXUI-based textual tester frontend |
| `REST` | `EasyLocal::REST` | reserved future HTTP/JSON application adapter |

`ConfigTOML` is implemented with `toml++` 3.4.x and `TUI` with FTXUI 7.x.
`ConfigYAML`, `Logging`, and `REST` remain reserved future integrations. A
future repository split may move ConfigTOML and TextUI into companion projects
without changing the direction of these dependencies.

## Dependency resolution

An optional adapter follows this order when it is enabled:

1. try to use an already installed package through `find_package`;
2. if the package is unavailable and `EASYLOCAL_FETCH_DEPENDENCIES=ON`, the
   adapter may use an explicit `FetchContent` fallback;
3. otherwise configuration fails with a diagnostic naming the missing package
   and the EasyLocal feature that requested it.

`EASYLOCAL_FETCH_DEPENDENCIES` defaults to `OFF`. Configuring EasyLocal must
never initiate network access unless the caller explicitly enables that option.
When an optional dependency is fetched, the installed EasyLocal component must
remain self-contained and relocatable. The adapter may install a private copy of
header-only dependency artifacts under the EasyLocal install tree instead of
assuming that a FetchContent subproject provides install rules. When the
dependency was found in the system instead, the installed EasyLocal component
retains a normal `find_dependency` requirement on that external package.
Vendoring third-party source into the EasyLocal repository is not the default
strategy and requires a separate design decision.

Feature switches use `EASYLOCAL_ENABLE_<FEATURE>` names and are `OFF` by
default when the feature introduces a third-party dependency. ConfigTOML first
tries `find_package(tomlplusplus 3.4 CONFIG)` and may fetch pinned `v3.4.0`;
TextUI first tries `find_package(ftxui 7 CONFIG)` and may fetch pinned `v7.0.3`.
Both fallbacks require `EASYLOCAL_FETCH_DEPENDENCIES=ON`.

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

Only adapters built and installed by the producer may report their package
component as available. Requesting an unavailable component must fail during
`find_package`; it must not silently fall back to Core or download dependencies
in the consumer project. Optional targets are loaded lazily: requesting only
`COMPONENTS Core` does not create `EasyLocal::ConfigTOML` or `EasyLocal::TUI`,
even when the installation contains those components. Each optional component
is exported through its own target file.

## Architectural boundary

Third-party types must not leak into Core Runner, neighborhood, search, app,
check, Tester, parameter, or configuration-tree contracts. An adapter translates
between its external library and stable public EasyLocal APIs. In particular:

- optional adapters may include public `easylocal/...` headers, but must not
  reach into `easylocal/detail/...`;
- TOML/YAML adapters produce the same textual overrides used by the std-only
  configuration frontends;
- TextUI consumes `app`/`Tester`/`check` facilities rather than becoming part
  of Runner or search algorithms;
- a future REST adapter must own its HTTP/JSON/server concerns outside Core and
  translate requests into the same public app/runtime operations used by other
  frontends; no HTTP or JSON type may enter a Core signature;
- a logging integration must not make Core depend on the selected logging or
  formatting library.

The boundary is checked by a deterministic architecture test: Core headers may
not include TextUI, ConfigTOML, FTXUI, or toml++, and optional adapter headers
may not include `easylocal/detail/*`. This keeps
`#include <easylocal/easylocal.hpp>` and `EasyLocal::Core` usable in a pure
standard-library consumer regardless of which optional integrations exist.

Materialized apps and bound runners borrow an Input by `const&`. Input is the
canonical application-boundary term (`input_type`, `input()`); the older
`instance_type`/`instance()` spelling remains available where needed for
compatibility. Binding a temporary Input is rejected at compile time so an
adapter cannot accidentally create a runtime that outlives its Input.

## ConfigTOML

`EasyLocal::ConfigTOML` is the first concrete optional adapter. It links
`EasyLocal::Core` and `tomlplusplus::tomlplusplus`, but the reverse dependency
does not exist. The adapter header is installed only when the feature is built:

```cpp
#include <easylocal/config/toml.hpp>

auto source = easylocal::config::load_toml_file("solver.toml");
```

TOML tables are flattened to the same dotted paths consumed by S27a. Native
strings, integers, floating-point values, booleans, and numeric/bool arrays are
converted to owned textual overrides. TOML date/time values, arrays of tables,
and string arrays are rejected with adapter diagnostics rather than silently
changing semantics. Typed lookup, parsing, cross-field validation, and atomic
commit remain the responsibility of the existing `apply_overrides` layer.

CI verifies `ConfigTOML` through both supported dependency providers: a
system-installed toml++ with fetching disabled, and a forced FetchContent path
with `CMAKE_DISABLE_FIND_PACKAGE_tomlplusplus=TRUE`. The system-provider install
keeps a normal `find_dependency(tomlplusplus)` requirement. The FetchContent
provider installs a private copy of toml++ headers under
`include/easylocal/third_party/tomlplusplus`, and the installed adapter exposes
that include root without requiring a separately installed toml++ package. The
installed-package consumer is exercised in both modes so optional-component
packaging is tested as well as source-tree compilation.


## TextUI

`EasyLocal::TUI` is an optional FTXUI 7.x adapter. It is installed only when
`EASYLOCAL_ENABLE_TUI=ON` and is discovered explicitly with:

```cmake
find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core TUI)
target_link_libraries(my_tester PRIVATE EasyLocal::TUI)
```

The installed TUI target is exported separately from Core. When FTXUI comes
from the system, the component retains a normal `find_dependency(ftxui 7)`
requirement. When it comes from FetchContent, FTXUI's installed package is used
from the EasyLocal installation prefix. In either case, a Core-only consumer
loads neither FTXUI nor `EasyLocal::TUI`.

## Future REST adapter

REST is deliberately not part of Core. Its eventual implementation should be a
thin application adapter over public EasyLocal contracts: parse/validate HTTP
and JSON externally, own request/response DTOs externally, keep Input lifetime
inside the adapter, then materialize `app.for_input(input)` and invoke runners or
Tester/check facilities as appropriate. This lets REST and TextUI evolve or be
spun off without changing search semantics or pulling server dependencies into
EasyLocal::Core.
