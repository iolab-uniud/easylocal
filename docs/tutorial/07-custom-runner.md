# 7. Writing your own runner

A search algorithm is a class with a single `run` member. This one samples
random moves and keeps the improving ones:

<!-- snippet: tutorial/tsp.hpp:custom-runner -->
```cpp
struct RandomDescentParameters
{
    easylocal::limit max_evaluations{1000}; // a count, or easylocal::unlimited

    // The schema makes the parameters configurable (chapter 9).
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<
                "max_evaluations",
                &RandomDescentParameters::max_evaluations>(
                "Evaluation budget",
                easylocal::config::range(1, easylocal::unlimited)));
    }

    easylocal::config::validation_result validate() const
    {
        return easylocal::config::check_schema(*this);
    }
};

class RandomDescent : public easylocal::parameters_base<RandomDescentParameters>
{
public:
    using parameters_base::parameters_base; // configurable, built from the block

    // The result type is the one run.finish() returns: auto deduces it.
    template<class Run, std::uniform_random_bit_generator RNG>
    auto run(Run& run, Run::solution_type solution, RNG& rng) const
    {
        run.limit_evaluations(parameters().max_evaluations);
        auto current = run.start(solution); // evaluates, emits run_started

        // should_stop: cancelled, the budget spent, the target reached or the
        // time up.
        while (!run.should_stop())
        {
            auto move = run.random_move(solution, rng);
            if (!move) // no move at all: a local optimum
                return run.finish(
                    std::move(solution),
                    current.cost(),
                    easylocal::termination_reason::local_optimum);

            run.next_iteration();
            auto candidate = run.evaluate_move(solution, current, *move);
            // A descent only ever improves: every commit is a new incumbent,
            // which commit_improvement() reports to the trace.
            if (run.better(candidate.cost(), current.cost()))
                run.commit_improvement(solution, current, std::move(candidate), *move);
        }
        return run.finish(std::move(solution), current.cost()); // run_finished
    }
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
| `should_stop()` | true on cancellation, a reached target cost, an exhausted evaluation budget or a passed time limit; records the reason |
| `moves(solution)`, `random_move(solution, rng)` | neighborhood access |
| `evaluate_move(solution, current, move)` | counts, emits `move_evaluated`, reports progress |
| `commit(solution, current, candidate, move)` | applies the move, emits `move_accepted` |
| `commit_improvement(solution, current, candidate, move)` | applies the move, emits `move_accepted` and `incumbent_updated`, for an algorithm whose current solution is its best |
| `next_iteration()` | advances the iteration counter |
| `incumbent_updated(previous, cost)` | for algorithms that keep a best-so-far solution |
| `finish(solution, cost[, reason])` | emits `run_finished`, returns a `search_result` |

Every runner is cancellable by contract: checking `should_stop()` in the loop is
all it takes. Per-run state lives in local variables, so `run` is `const` and a
runner can be reused.

A descent only ever moves to a better solution, so its current solution is also
its best: it commits with `commit_improvement()`, which emits `incumbent_updated`
as well. An algorithm that also accepts worsenings (Simulated Annealing) commits
with `commit()` and keeps its best apart, with `best_so_far`.

The runner is used like a built-in one:

<!-- snippet: tutorial/main.cpp:custom-runner-use -->
```cpp
auto descent =
    el::make_runner<RandomDescent>(RandomDescentParameters{.max_evaluations = 200})
    | sm | nhe;
```

`parameters_type`, which `easylocal::parameters_base<RandomDescentParameters>`
declares, names the parameters the algorithm is constructed from; the base
also keeps the block and gives it back through `parameters()`. Its
`parameter_schema()` and `validate()` (chapter 9) make it a parameter block,
and that is the whole rule of configuration, for a runner as for the other
configurable classes: the runner holds the parameters, builds the algorithm
from them when it is bound, and gives them as `search.*` to the command line,
configuration files and the interactive tester; an app (chapter 11) gives them
as `runners.<name>.*`. An algorithm whose `parameters_type` is not a block
still runs in an app, but no frontend can change its parameters, and
`check(app, ...)` (chapter 14) reports it.

## See also

- [Runners](../reference/runners.md#search_run): the complete `search_run`
  interface and its escape hatches.

## Next steps

[Chapter 8](08-solvers.md) lets the framework build the initial solution.
