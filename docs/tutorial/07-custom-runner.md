# 7. Writing your own runner

A search algorithm is a class with a single `run` member. This one samples
random moves and keeps the improving ones:

<!-- snippet: tutorial/tsp.hpp:custom-runner -->
```cpp
struct RandomDescentParameters
{
    easylocal::limit max_evaluations{1000}; // a count, or easylocal::unlimited
};

class RandomDescent
{
public:
    using parameters_type = RandomDescentParameters; // for app registration

    explicit RandomDescent(RandomDescentParameters parameters) : parameters_{parameters}
    {
    }

    // The result type is the one run.finish() returns: auto deduces it.
    template<class Run, std::uniform_random_bit_generator RNG>
    auto run(Run& run, Run::solution_type solution, RNG& rng) const
    {
        run.limit_evaluations(parameters_.max_evaluations);
        auto current = run.start(solution); // evaluates, emits run_started

        while (!run.should_stop()) // cancellation or exhausted budget
        {
            auto move = run.random_move(solution, rng);
            if (!move)
                break;

            run.next_iteration();
            auto candidate = run.evaluate_move(solution, current, *move);
            if (run.better(candidate.cost(), current.cost()))
                run.commit(solution, current, std::move(candidate), *move);
        }
        return run.finish(std::move(solution), current.cost()); // run_finished
    }

private:
    RandomDescentParameters parameters_;
};
```

The framework calls `run` with an `easylocal::search_run`. It gives access to
the search context (`better`, `neighborhood_explorer`, ...) and owns what every
search shares: counters, the evaluation budget, cancellation, progress reporting
and the trace events. The algorithm only describes its logic:

| `search_run` member | Effect |
| --- | --- |
| `limit_evaluations(n)` | evaluation budget, including the initial evaluation: a count, or `easylocal::unlimited` |
| `start(solution)` | first evaluation; emits `run_started`, reports progress |
| `should_stop()` | true on cancellation or exhausted budget; records the reason |
| `moves(solution)`, `random_move(solution, rng)` | neighborhood access |
| `evaluate_move(solution, current, move)` | counts, emits `move_evaluated`, reports progress |
| `commit(solution, current, candidate, move)` | applies the move, emits `move_accepted` |
| `next_iteration()` | advances the iteration counter |
| `incumbent_updated(previous, cost)` | for algorithms that keep a best-so-far solution |
| `finish(solution, cost[, reason])` | emits `run_finished`, returns a `search_result` |

Every runner is cancellable by contract: checking `should_stop()` in the loop is
all it takes. Per-run state lives in local variables, so `run` is `const` and a
runner can be reused.

The runner is used like a built-in one:

<!-- snippet: tutorial/main.cpp:custom-runner-use -->
```cpp
auto descent =
    el::make_runner<RandomDescent>(RandomDescentParameters{.max_evaluations = 200})
    | sm | nhe;
```

`parameters_type` is only needed to register the runner in an app (chapter 11).
When the parameters also describe themselves, with a `parameter_schema()` and
a `validate()` as in chapter 9, nothing else is needed to make them
configurable: the runner holds them, builds the algorithm from them when it is
bound, and gives them as `search.*` to the command line, configuration files
and the interactive tester.

## See also

- [Runners](../reference/runners.md#search_run): the complete `search_run`
  interface and its escape hatches.

## Next steps

[Chapter 8](08-solvers.md) lets the framework build the initial solution.
