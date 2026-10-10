# 15. A REST service

The **REST** adapter makes the same app available over HTTP. Clients can submit
runs, monitor progress, cancel searches and retrieve solutions without changes
to the problem components. A bounded worker pool runs searches separately from
the HTTP threads.

Enable the optional `REST` component, based on Crow:

```cmake
find_package(EasyLocal CONFIG REQUIRED COMPONENTS Core REST)
target_link_libraries(tsp_rest PRIVATE EasyLocal::REST)
```

## Without a codec

The quickest setup reuses the I/O hooks from chapter 5. Inputs and tours travel
as JSON strings containing your existing text format; this TSP's numeric costs
travel as JSON numbers. No extra serialization code is needed:

<!-- snippet: tutorial/rest_main.cpp:text -->
```cpp
// The Input and the tours in the text of the I/O hooks (chapter 5).
auto text_api = el::rest::blueprint("/tsp-text", application);
```

```sh
$ curl -X POST localhost:18080/tsp-text/runners/sa/runs \
       -H 'Content-Type: application/json' \
       -d '{"input": "5\n0 2 9 10 7\n2 0 6 4 3\n9 6 0 8 5\n10 4 8 0 6\n7 3 5 6 0\n", "seed": 7}'
{"id":"1","runner":"sa","seed":7,"status":"queued",...}

$ curl localhost:18080/tsp-text/runs/1/solution
{"solution":"...","cost":26,...}
```

## The codec

A **codec** lets clients use structured JSON instead of text strings. Define
only the conversions you need; values without a codec conversion still use
their I/O hooks:

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
        const auto& rows = payload["distance"];
        if (rows.t() != crow::json::type::List)
            throw std::invalid_argument{"'distance' must be a list of rows"};
        tutorial::Tsp tsp;
        for (const auto& row : rows)
        {
            if (row.t() != crow::json::type::List)
                throw std::invalid_argument{"'distance' must be a list of rows"};
            auto& values = tsp.distance.emplace_back();
            for (const auto& value : row)
            {
                if (value.t() != crow::json::type::Number)
                    throw std::invalid_argument{"'distance' must hold numbers"};
                values.push_back(value.d());
            }
        }
        for (const auto& values : tsp.distance)
            if (values.size() != tsp.cities())
                throw std::invalid_argument{"'distance' must be a square matrix"};
        // The 2-opt delta assumes that a reversed segment costs the same.
        for (std::size_t i = 0; i < tsp.cities(); ++i)
            for (std::size_t j = 0; j < i; ++j)
                if (tsp.distance[i][j] != tsp.distance[j][i])
                    throw std::invalid_argument{"'distance' must be symmetric"};
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
| `decode_input(const crow::json::rvalue&) -> Input` | without `read_input` | the `input` of a run request; throw `std::invalid_argument` to reject it |
| `encode_solution(const Input&, const Solution&) -> crow::json::wvalue` | without `write_solution` | the solution of a finished run |
| `encode_cost(const Cost&) -> crow::json::wvalue` | for a cost that is not text | its cost |
| `decode_initial_solution(const Input&, const crow::json::rvalue&) -> Solution` | no | accepts an `initial_solution` in the request, as `read_solution` does from a string; without either such requests are rejected, and runs start as their `start` says |
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

// The Input and the tours in the text of the I/O hooks (chapter 5).
auto text_api = el::rest::blueprint("/tsp-text", application);

// The same app, in the JSON of TspCodec.
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
server.register_blueprint(text_api.crow_blueprint());
server.register_blueprint(api.crow_blueprint());
// Listening on this machine only; a port that cannot be bound is an error.
auto serving = server.bindaddr("127.0.0.1").port(port).multithreaded().run_async();
server.wait_for_server_start();
if (serving.wait_for(std::chrono::milliseconds{200}) == std::future_status::ready)
{
    std::cerr << "error: cannot listen on 127.0.0.1:" << port << '\n';
    return 1;
}
serving.get(); // until a signal stops the server
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
| `GET /tsp/parameters` | the parameters a run may set, with their values |
| `POST /tsp/runners/<runner>/runs` | enqueue a run (`202`, with a `Location`) |
| `GET /tsp/runs/<id>` | status and progress |
| `GET /tsp/runs/<id>/solution` | the solution and its cost |
| `POST /tsp/runs/<id>/cancel` | cooperative cancellation |
| `DELETE /tsp/runs/<id>` | forget a finished run |

Set `seed` to reproduce a stochastic run, including its random start. Without
an explicit seed, the service uses `blueprint_options::seed` plus the run id.
Runs start from a random tour unless the request supplies `initial_solution`
or `"start": "initial"`.

A `target` stops the search when it reaches that cost or better. For this
TSP, `"target": 26` stops at the known optimum of the five-city instance.

## See also

- [REST](../rest.md): the complete HTTP contract, errors and the concurrency
  model.

## Next steps

[Chapter 16](16-observing-and-controlling.md) watches and stops a run from your
own code.
