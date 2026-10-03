# 14. A REST service

The **REST** adapter exposes an app as an HTTP API: clients submit runs, poll
their status and progress, cancel them and fetch the solutions. Runs execute on
a bounded pool of background workers, separate from the HTTP threads. It is the
optional `REST` component (Crow):

```cmake
find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core REST)
target_link_libraries(tsp_rest PRIVATE EasyLocal::REST)
```

## The codec

The app works with problem values; a **codec** translates them from and to
JSON:

<!-- snippet: tutorial/rest_main.cpp:codec -->
```cpp
// Translates between JSON and the problem values.
struct TspCodec
{
    // {"distance": [[0, 2, 9, 10, 7], [2, 0, 6, 4, 3], ...]}, one array per row
    tutorial::Tsp decode_input(const crow::json::rvalue& payload) const
    {
        if (payload.t() != crow::json::type::Object || !payload.has("distance"))
            throw std::invalid_argument{"input must have 'distance'"};
        tutorial::Tsp tsp;
        for (const auto& row : payload["distance"])
        {
            auto& values = tsp.distance.emplace_back();
            for (const auto& value : row)
                values.push_back(value.d());
        }
        for (const auto& values : tsp.distance)
            if (values.size() != tsp.cities())
                throw std::invalid_argument{"'distance' must be a square matrix"};
        return tsp;
    }

    crow::json::wvalue encode_solution(
        const tutorial::Tsp&,
        const tutorial::Tour& tour) const
    {
        crow::json::wvalue json;
        json["order"] = tour.order;
        return json;
    }

    crow::json::wvalue encode_cost(double length) const
    {
        crow::json::wvalue json;
        json["length"] = length;
        return json;
    }
};
```

| Member | Required | Purpose |
| --- | --- | --- |
| `decode_input(const crow::json::rvalue&) -> Input` | yes | the `input` of a run request; throw `std::invalid_argument` to reject it |
| `encode_solution(const Input&, const Solution&) -> crow::json::wvalue` | yes | the solution of a finished run |
| `encode_cost(const Cost&) -> crow::json::wvalue` | yes | its cost |
| `decode_initial_solution(const Input&, const crow::json::rvalue&) -> Solution` | no | accepts an `initial_solution` in the request; without it such requests are rejected, and runs start from the SolutionManager's `initial_solution()` |
| `decode_cost(const crow::json::rvalue&) -> Cost` | no | accepts a `target` in the request; without it a target is a JSON number, for arithmetic costs only |

## The service

<!-- snippet: tutorial/rest_main.cpp:rest -->
```cpp
auto application = el::app("tsp")
    | (el::solution_manager<TourManager>() | el::component<TourLength>())
    | (el::neighborhood<TwoOptExplorer>()
        | el::delta<TourLength, TwoOptLengthDelta>())
    | el::runner<runners::FirstImprovement>("fi")
    | el::runner<runners::SimulatedAnnealing<Classic>>("sa");

auto api = el::rest::blueprint(
    "/tsp",
    std::move(application),
    TspCodec{},
    el::rest::blueprint_options{
        .workers = 2,         // background search threads
        .queue_capacity = 16, // waiting runs
        .seed = 2026,         // runs without an explicit "seed"
    });

crow::SimpleApp server;
server.register_blueprint(api.crow_blueprint());
server.port(port).multithreaded().run();
```

The program is `examples/tutorial/rest_main.cpp`, built when the REST component
is enabled (`-DEASYLOCAL_ENABLE_REST=ON`); a test starts it and runs the
session below.

```sh
$ curl -X POST localhost:18080/tsp/runners/sa/runs \
       -H 'Content-Type: application/json' \
       -d '{"input": {"distance": [[0,2,9,10,7], [2,0,6,4,3], [9,6,0,8,5], [10,4,8,0,6], [7,3,5,6,0]]}, "seed": 7}'
{"id":"1","runner":"sa","seed":7,"status":"queued",...}

$ curl localhost:18080/tsp/runs/1
{"id":"1","status":"succeeded","progress":{"evaluations":...},...}

$ curl localhost:18080/tsp/runs/1/solution
{"solution":{"order":[...]},"cost":{"length":26},...}
```

| Route | Meaning |
| --- | --- |
| `GET /tsp/` | application metadata |
| `GET /tsp/runners` | registered runner names |
| `POST /tsp/runners/<runner>/runs` | enqueue a run (`202`, with a `Location`) |
| `GET /tsp/runs/<id>` | status and progress |
| `GET /tsp/runs/<id>/solution` | the solution and its cost |
| `POST /tsp/runs/<id>/cancel` | cooperative cancellation |
| `DELETE /tsp/runs/<id>` | forget a finished run |

A run's `seed` makes stochastic runs reproducible; without one, a run uses
`blueprint_options::seed` plus its id. A `target`, such as a known lower bound,
stops the run as soon as its cost is at least as good: for the tutorial's
`double` tour length, `"target": 23.0`.

## See also

- [REST](../rest.md): the complete HTTP contract, errors and the concurrency
  model.

## Next steps

[Chapter 15](15-observing-and-controlling.md) watches and stops a run from your
own code.
