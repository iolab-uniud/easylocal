# REST application adapter

`EasyLocal::REST` is an optional Crow-based application adapter. It exposes a
configured EasyLocal `app` as a generic Crow `Blueprint` while keeping HTTP,
JSON, Crow, and server lifecycle completely outside `EasyLocal::Core`.

The design follows one rule that is also used by the TextUI:

> one run owns one fresh mutable EasyLocal runtime; only the immutable Input may
> be shared across runs.

This keeps concurrency at the application boundary instead of making
`app_instance`, SolutionManager, neighborhoods, algorithms, or Runner state
internally synchronized.

## Enabling the component

REST is disabled by default:

```sh
cmake -S . -B build/rest \
  -DEASYLOCAL_ENABLE_REST=ON
```

EasyLocal first looks for Crow 1.3 through `find_package`. If it is unavailable,
a pinned Crow `v1.3.3` plus standalone Asio `1.38.2` may be fetched only when
`EASYLOCAL_FETCH_DEPENDENCIES=ON` is explicitly enabled.

Installed consumers request the component explicitly:

```cmake
find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core REST)
target_link_libraries(my_server PRIVATE EasyLocal::REST)
```

A consumer requesting only `Core` neither creates `EasyLocal::REST` nor loads
Crow/Asio.

## Generic app Blueprint

The REST adapter intentionally does not own a Crow server. It produces a
Blueprint that can be mounted into an existing Crow application, leaving port,
concurrency, middleware, logging, and process lifecycle to normal Crow code.

A codec supplies the domain-specific JSON boundary:

```cpp
struct AssignmentCodec
{
    auto decode_input(const crow::json::rvalue& json) const -> Input;

    auto encode_solution(
        const Input& input,
        const Solution& solution) const -> crow::json::wvalue;

    auto encode_cost(const Cost& cost) const -> crow::json::wvalue;

    // Optional. If omitted, the SolutionManager initial_solution() capability
    // is used when available.
    auto decode_initial_solution(
        const Input& input,
        const crow::json::rvalue& json) const -> std::optional<Solution>;
};
```

The generic Blueprint is then mounted explicitly:

```cpp
auto api = easylocal::rest::blueprint(
    "/assignment",
    application,
    AssignmentCodec{},
    {.workers = 4, .queue_capacity = 32});

crow::SimpleApp server;
server.register_blueprint(api.crow_blueprint());

server.port(8080)
      .multithreaded()
      .run();
```

A complete compiling example is available as `easylocal_assignment_rest_mwe`
under `examples/assignment`. It decodes `demand`/`capacity` arrays from Crow
JSON and serializes the final assignment plus hierarchical cost.

`app_blueprint` is deliberately non-copyable/non-movable because its Crow route
callbacks refer to its state. Keep it alive for at least as long as the Crow
application uses the registered Blueprint.

The codec is called behind an adapter-owned mutex, so the codec itself does not
have to provide internal synchronization.

## Routes

For a Blueprint mounted at `/assignment`, the current generic surface is:

| Method | Route | Meaning |
| --- | --- | --- |
| `GET` | `/assignment/` | application metadata and registered runners |
| `GET` | `/assignment/runners` | registered runner names |
| `POST` | `/assignment/runners/<runner>/runs` | enqueue a new run |
| `GET` | `/assignment/runs/<id>` | inspect run state |
| `GET` | `/assignment/runs/<id>/solution` | retrieve completed solution and cost |
| `DELETE` | `/assignment/runs/<id>` | request cooperative stop for an active run, or discard a terminal run |

A successful submission returns `202 Accepted`. A full bounded execution queue
returns `503`. Invalid JSON returns `400`; an unknown runner returns `404`; a
request that cannot provide/build an initial solution returns `422`.

Runs move through:

```text
queued -> running -> succeeded
   |         |      -> failed
   +---------+------> cancelled
```

Built-in searches support cooperative cancellation. `DELETE` on a queued or
running cooperative run requests stop and returns `202 Accepted`; the run record
remains queryable until explicitly deleted. If the search has already produced
a partial solution, `/solution` remains available after cancellation. A custom
runner that does not accept `run_control` remains fully executable but reports
`stoppable: false`; attempting to cancel it returns `409`.

`GET /runs/<id>` also reports `evaluations`, `iterations`, optional
`evaluation_limit`, and `stoppable`. These fields are observation state owned by
the REST adapter, not mutable state shared with a Core runtime.

## Concurrency model

Crow concurrency and solver concurrency are intentionally separate:

```text
             Crow HTTP workers
                    |
       parse / route / enqueue / reply
                    |
                    v
         bounded REST execution pool
           |         |         |
         run A     run B     run C
           |         |         |
      fresh runtime fresh runtime fresh runtime
           \         |         /
             immutable Input
```

Crow request threads do not execute a CPU-bound local search to completion.
They decode the request, create the immutable Input snapshot, enqueue work, and
return the run identifier.

Each accepted job obtains a snapshot of the configured `app` and invokes the
Core fresh-runtime execution primitive. Mutable SolutionManager, neighborhood,
algorithm, Runner, RNG, and Solution state therefore belongs to that run only.
No mutex is added to those Core objects and no `Clone()` protocol is required.

The execution queue is bounded so a server can apply explicit resource limits.
The default worker count is `max(1, hardware_concurrency() - 1)` and the default
waiting-queue capacity is 64; both are adapter options rather than Core policy.

## Cooperative control and TextUI

Core exposes a small std-only control object:

```cpp
std::stop_source stop;
auto observer = [](const easylocal::run_progress& progress) { /* observe */ };
easylocal::run_control control{stop.get_token(), observer};
```

The observer is non-owning and valid only for the duration of the controlled
run. Built-in algorithms provide a separate controlled overload, so ordinary
`run(...)` keeps its previous zero-control-overhead path. Custom algorithms may
opt in by accepting `const easylocal::run_control&` as the final runtime
argument; algorithms that do not opt in are executed through the legacy path.

The same isolation rule is used for an interactive background run. TextUI keeps
the FTXUI event loop on the UI thread, snapshots the app/Input/current Solution,
and runs the search on a `std::jthread` through the fresh-runtime Core API. The
`jthread` stop token feeds `run_control`; progress is bridged back as FTXUI custom
events. Stop never mutates Tester/runtime state from the worker thread, and a
cooperatively stopped partial solution is committed only on the UI thread.

Thus REST and TextUI exercise the same architectural boundary without sharing a
threading subsystem:

```text
Core:     fresh isolated runtime per run
TextUI:   one background run owned by the frontend
REST:     bounded pool of background runs owned by the adapter
```

## Security boundary

`EasyLocal::REST` is not intended to be an Internet edge/security framework.
TLS termination, authentication, authorization, request-rate limiting, reverse
proxy policy, ingress controls, and deployment hardening belong to the network
infrastructure around the process.

The adapter still performs ordinary protocol/application validation and relies
on maintained Crow/Asio HTTP parsing. Queue bounds are resource-control
semantics, not a substitute for edge security.

No Crow, Asio, HTTP, or JSON type appears in a Core signature. The architecture
test also prevents optional adapters from reaching into `easylocal/detail/*`.
