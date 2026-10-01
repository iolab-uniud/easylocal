# External dependency policy

EasyLocal keeps its algorithmic and configuration core independent from
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
| `REST` | `EasyLocal::REST` | Crow-based HTTP/JSON application adapter |

`ConfigTOML` is implemented with `toml++` 3.4.x, `TUI` with FTXUI 7.x, and
`REST` with Crow 1.3.x plus standalone Asio. `ConfigYAML` and `Logging` remain
reserved future integrations. A future repository split may move ConfigTOML,
TextUI, and REST into companion projects without changing the direction of these
dependencies.

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
REST first tries `find_package(Crow 1.3 CONFIG)` and, when explicitly allowed to
fetch dependencies, uses pinned Crow `v1.3.3` plus standalone Asio `1.38.2`. All
fallbacks require `EASYLOCAL_FETCH_DEPENDENCIES=ON`.

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
`COMPONENTS Core` does not create `EasyLocal::ConfigTOML`, `EasyLocal::TUI`, or
`EasyLocal::REST`, even when the installation contains those components. Each
optional component is exported through its own target file.

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
- REST owns all Crow/HTTP/JSON/server concerns outside Core and translates
  requests into public app/runtime operations; no Crow, HTTP, or JSON type may
  enter a Core signature;
- a logging integration must not make Core depend on the selected logging or
  formatting library.

The boundary is checked by a deterministic architecture test: Core headers may
not include TextUI, ConfigTOML, REST, FTXUI, toml++, or Crow, and optional adapter
headers may not include `easylocal/detail/*`. This keeps
`#include <easylocal/easylocal.hpp>` and `EasyLocal::Core` usable in a pure
standard-library consumer regardless of which optional integrations exist.

Materialized apps and bound runners borrow an Input by `const&`. Input is the
application-boundary term (`input_type`, `input()`). Binding a temporary Input
is rejected at compile time so an adapter cannot accidentally create a runtime that outlives its Input.

## ConfigTOML

`EasyLocal::ConfigTOML` is the first concrete optional adapter. It links
`EasyLocal::Core` and `tomlplusplus::tomlplusplus`, but the reverse dependency
does not exist. The adapter header is installed only when the feature is built:

```cpp
#include <easylocal/adapters/toml.hpp>

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

## REST

`EasyLocal::REST` is an optional Crow 1.3.x adapter. It is installed only when
`EASYLOCAL_ENABLE_REST=ON` and is discovered explicitly with:

```cmake
find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core REST)
target_link_libraries(my_server PRIVATE EasyLocal::REST)
```

The adapter produces a generic Crow Blueprint for a configured EasyLocal `app`;
it does not own the Crow server. Crow request threads parse/route/enqueue work,
while CPU-bound search runs execute on a separate bounded adapter-owned pool.
Every run materializes fresh mutable EasyLocal runtime state and may share only
the immutable Input. This is the same isolation rule used by asynchronous
TextUI runs and avoids making Core search objects internally synchronized.

When Crow is provided by the system, the installed component keeps the normal
Crow package dependency. With the explicit FetchContent path, the pinned Crow
package and standalone Asio headers are installed with the EasyLocal REST
component so the result remains relocatable. A Core-only consumer loads neither
Crow/Asio nor `EasyLocal::REST`.

TLS, authentication/authorization, rate limiting, reverse-proxy policy, and
Internet-edge hardening are deliberately outside the adapter and belong to the
deployment infrastructure. See [`rest.md`](rest.md) for the routes, codec
contract, lifetime rules, and concurrency model.
