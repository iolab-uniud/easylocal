# 7. Writing your own runner

To try a search strategy of your own, write a class with a `run` member.
The framework supplies evaluation, counters and run control, leaving you to
decide which moves to accept. This example samples random moves and keeps
improvements:

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

The bound runner passes an `easylocal::search_run` to `run`. It provides
comparison and neighborhood access, and handles the work shared by algorithms:

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

Check `should_stop()` in the loop to honour cancellation and run limits.
Keep per-run state in local variables so `run` can be `const` and the algorithm
can be reused.

In a descent, the current solution is also the best found. Use
`commit_improvement()` to apply a move and report the new incumbent. Algorithms
that accept worsening moves, such as Simulated Annealing, use `commit()` and
track the best solution separately with `best_so_far`.

The runner is used like a built-in one:

<!-- snippet: tutorial/main.cpp:custom-runner-use -->
```cpp
auto descent =
    el::make_runner<RandomDescent>(RandomDescentParameters{.max_evaluations = 200})
    | sm | nhe;
```

`parameters_base<RandomDescentParameters>` declares `parameters_type`, stores
the parameters and exposes them through `parameters()`. The runner keeps this
block and constructs the algorithm from it when bound.

Adding `parameter_schema()` and `validate()` to the block (chapter 9) also
exposes it to configuration files and frontends. Standalone runners use paths
under `search.*`; apps use `runners.<name>.*` (chapter 11). Your custom
algorithm then gets the same configuration support as the built-in ones.

An algorithm whose `parameters_type` is not a parameter block can still run,
but frontends cannot edit its settings. `check(app, ...)` reports this gap
(chapter 14).

## See also

- [Runners](../reference/runners.md#search_run): the complete `search_run`
  interface and its escape hatches.

## Next steps

[Chapter 8](08-solvers.md) lets the framework build the initial solution.

For a worked extension of an existing algorithm, the advanced
[Simulated Annealing chapter](18-customizing-annealing.md) reads the library's
search loop and implements a custom reheating policy.
