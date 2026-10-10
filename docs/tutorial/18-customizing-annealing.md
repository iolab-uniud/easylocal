# 18. Advanced: customizing Simulated Annealing

Simulated Annealing separates three decisions: how to propose a move, whether
to accept it, and how to change the temperature. The neighborhood handles the
first; acceptance and temperature policies handle the other two. You can
change a policy while keeping the search loop, delta evaluation, best solution,
run limits and tracing.

This chapter reads the actual EL4 implementation, enables its built-in
reheating schedule, then writes a different reheating rule. It builds on
[runners](05-running-a-search.md), [custom algorithms](07-custom-runner.md)
and [parameter blocks](09-configuration.md). The complete program is
`examples/tutorial/annealing_main.cpp`; the custom policy is in
`reheat_on_rejection.hpp` beside it.

## Inside the search loop

The algorithm is `runners::SimulatedAnnealing<TemperaturePolicy, Acceptance>`.
Its defaults are `temperature::Classic` and `MetropolisAcceptance`.
Here is its `run` member, taken directly from
`include/easylocal/runners/simulated_annealing.hpp`. This is the implementation
used by the library, including calibration and temperature tracing:

<!-- snippet: include/easylocal/runners/simulated_annealing.hpp:annealing-run -->
```cpp
template<class Run, std::uniform_random_bit_generator RNG>
    requires detail::simulated_annealing_context<
        typename Run::context_type, Acceptance, RNG>
[[nodiscard]]
auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
{
    static_assert(
        detail::delta_agrees_with_better<typename Run::context_type>(),
        "Simulated Annealing decides with the sign of cost::delta, which"
        " must agree with better(): the compare of the cost expression "
        "finds a larger cost better");
    run.limit_evaluations(max_evaluations_);
    auto temperature = temperature_policy_;
    auto current = run.start(solution);
    if constexpr (calibrating_temperature_policy<TemperaturePolicy>)
        calibrate(run, temperature, solution, current, rng);
    temperature.reset();
    // The temperature as the trace last saw it, for a tracer that
    // observes its changes.
    constexpr bool traces_temperature =
        trace::observes<typename Run::tracer_type, trace::event::temperature_changed>;
    [[maybe_unused]] double traced_temperature = 0.0;
    if constexpr (traces_temperature)
        traced_temperature = report_temperature(run, 0.0, temperature.temperature());

    best_so_far best{solution, current.cost()};

    while (!temperature.finished() && !run.should_stop())
    {
        auto move = run.random_move(solution, rng);
        if (!move.has_value())
        {
            // No move to try: as Hill Climbing, a local optimum.
            return run.finish(
                std::move(best.solution),
                std::move(best.cost),
                termination_reason::local_optimum);
        }

        run.next_iteration();
        auto candidate = run.evaluate_move(solution, current, *move);

        const auto accepted = static_cast<bool>(acceptance_.accept(
            candidate.cost(),
            current.cost(),
            temperature.temperature(),
            rng));

        if (accepted)
        {
            run.commit(solution, current, std::move(candidate), *move);

            best.update(run, solution, current);
        }

        temperature.on_iteration(accepted);
        if constexpr (traces_temperature)
        {
            if (temperature.temperature() != traced_temperature)
            {
                traced_temperature = report_temperature(
                    run,
                    traced_temperature,
                    temperature.temperature());
            }
        }
    }

    return run.finish(std::move(best.solution), std::move(best.cost));
}
```

Read it in four parts:

1. **Start.** Apply the evaluation limit and evaluate the initial solution.
   Copy the temperature policy for this run. If it supports calibration,
   estimate the initial temperature from sampled moves, then reset the schedule.
2. **Propose and evaluate.** Draw a move from the neighborhood and evaluate
   it through `search_run`. This uses the deltas attached to the recipe and
   counts the evaluation. An empty neighborhood ends the run.
3. **Accept and remember.** Ask `acceptance_.accept` whether to keep the move.
   Metropolis accepts non-worsening moves and accepts a finite worsening
   delta with probability `exp(-delta / temperature)`. After a commit,
   `best.update` records an improvement of the best solution found.
4. **Advance.** Tell the schedule whether the proposal was accepted. Report
   any temperature change, then continue until the schedule or run control
   stops the search. Return the best solution, which may differ from the last
   visited one.

`if constexpr` selects calibration and tracing at compile time. An unused
feature adds no branch to the proposal loop. The temperature and acceptance
types are also known at compile time, allowing their small functions to be
inlined.

The `calibrate` helper evaluates random moves at the starting solution without
applying them; those evaluations consume the same budget. `report_temperature`
emits `trace::event::temperature_changed`. The bookkeeping lives in
`search_run`, so a new policy automatically retains cancellation, progress,
evaluation limits and best-solution reporting.

## First, use the built-in reheating

A colder search accepts fewer worsening moves. Reheating raises the
temperature to let it explore again, while retaining the best solution found.
EL4 already supplies `temperature::Reheating<Descent>`: when a descent ends,
it starts another cooling phase. We can wrap `Classic` and reuse the TSP
recipes from the earlier chapters:

<!-- snippet: tutorial/annealing_main.cpp:annealing-problem -->
```cpp
const auto tsp = five_cities();
const auto sm =
    el::solution_manager<TourManager>().with_cost(el::component<TourLength>());
const auto nhe =
    el::neighborhood<TwoOptExplorer>().with_delta<TourLength, TwoOptLengthDelta>();
```

<!-- snippet: tutorial/annealing_main.cpp:scheduled-reheating -->
```cpp
using Reheated = runners::temperature::Reheating<runners::temperature::Classic>;
auto scheduled = el::make_runner<runners::SimulatedAnnealing<Reheated>>(
    {
        .temperature =
            {
                .descent =
                    {
                        .initial_temperature = 10.0,
                        .final_temperature = 0.1,
                        .cooling_rate = 0.9,
                        .samples_per_temperature = 20,
                    },
                .allowed_reheats = 2,
                .reheat_ratio = 0.5,
            },
        .max_evaluations = 5000,
    })
                     .with_solution_manager(sm)
                     .with_neighborhood(nhe);
```

This performs the first cooling phase from 10.0, then up to two more from
5.0 (`10.0 * reheat_ratio`). Reheating changes the temperature, not the current
tour. With `Classic`, each phase gets its own cooling schedule; the outer
`max_evaluations` still bounds the entire run. For a budgeted descent such as
`Hybrid`, the wrapper instead divides the descent's budget between the phases
(see [temperature policies](../reference/runners.md#built-in-algorithms)).

The named composition above also has the pipe spelling
`make_runner<Algorithm>(parameters) | sm | nhe`. Both build the same runner.

## A different rule: reheat after rejected moves

Suppose we want to reheat before the current cooling phase ends, after a
fixed number of consecutive rejected proposals. This is a change to the
temperature policy: the search already calls `on_iteration(accepted)` after
every proposal.

Our rule is:

- continue geometric cooling after every proposal;
- clear the rejection streak whenever a move is accepted;
- at the rejection threshold, restore the initial temperature and restart
  cooling, provided a reheat remains;
- after the reheat allowance is spent, let the final cooling phase end.

This measures **rejection**, not lack of improvement: an accepted equal-cost
or worsening move also clears the streak. Reheating may help exploration, but
does not guarantee a better solution. Compare schedules over several seeds
with the same evaluation budget.

### Describe the parameters

Reuse `ClassicParameters` as a nested group, then add the trigger and the
maximum number of reheats. The schema validates the threshold and makes both
settings available to configuration and tuning:

<!-- snippet: tutorial/reheat_on_rejection.hpp:rejection-parameters -->
```cpp
// The cooling schedule and the trigger for a bounded number of reheats.
struct ReheatOnRejectionParameters
{
    easylocal::runners::temperature::ClassicParameters descent{};
    std::size_t rejections_before_reheat{100};
    std::size_t allowed_reheats{2};

    static consteval auto parameter_schema()
    {
        namespace cfg = easylocal::config;
        return cfg::fields(
            cfg::group<"descent", &ReheatOnRejectionParameters::descent>(
                "The geometric cooling schedule"),
            cfg::field<
                "rejections_before_reheat",
                &ReheatOnRejectionParameters::rejections_before_reheat>(
                "Consecutive rejected proposals before reheating",
                cfg::range(1, easylocal::unlimited)),
            cfg::field<"allowed_reheats", &ReheatOnRejectionParameters::allowed_reheats>(
                "Maximum reheats (0: cooling only)",
                cfg::range(0, easylocal::unlimited)));
    }

    easylocal::config::validation_result validate() const
    {
        return easylocal::config::check_schema(*this);
    }
};
```

### Implement the temperature policy

The required interface has four members: `reset`, `temperature`,
`on_iteration` and `finished`. `initial_` keeps a fresh cooling schedule;
`descent_` is the one currently running. The two counters track consecutive
rejections and consumed reheats.

<!-- snippet: tutorial/reheat_on_rejection.hpp:rejection-policy!rejection-calibration -->
```cpp
// Restart geometric cooling after a streak of rejected proposals.
class ReheatOnRejection : public easylocal::parameters_base<ReheatOnRejectionParameters>
{
public:
    explicit ReheatOnRejection(const ReheatOnRejectionParameters& parameters)
        : parameters_base{easylocal::config::require_valid(parameters)},
          initial_{parameters.descent},
          descent_{initial_}
    {
    }

    void reset()
    {
        descent_ = initial_;
        descent_.reset();
        rejected_ = 0;
        reheats_ = 0;
    }

    double temperature() const
    {
        return descent_.temperature();
    }

    void on_iteration(bool accepted)
    {
        descent_.on_iteration(accepted);
        if (accepted)
            rejected_ = 0;
        else if (reheats_ < parameters().allowed_reheats
            && ++rejected_ >= parameters().rejections_before_reheat)
        {
            descent_ = initial_;
            descent_.reset();
            rejected_ = 0;
            ++reheats_;
        }
    }

    bool finished() const
    {
        return descent_.finished();
    }

    std::size_t reheats() const
    {
        return reheats_;
    }

private:
    easylocal::runners::temperature::Classic initial_;
    easylocal::runners::temperature::Classic descent_;
    std::size_t rejected_{};
    std::size_t reheats_{};
};
```

The custom logic is in `on_iteration`. It advances the underlying schedule,
then decides whether to replace it with a fresh copy. A reheat can therefore
also occur on the proposal that would otherwise finish cooling. If the
schedule ends before the rejection threshold is reached, the search ends
without using the remaining reheats.

Setting `allowed_reheats` to zero gives ordinary Classic cooling. Each phase
is finite, and the reheat count is bounded; an evaluation or time limit can
still stop the run earlier. Resetting the temperature never resets the run's
evaluation count or discards its best solution.

### Preserve optional calibration

Because the nested block exposes Classic's calibration settings, forward its
optional calibration interface too. These members are also part of
`ReheatOnRejection`; they were omitted from the block above for readability:

<!-- snippet: tutorial/reheat_on_rejection.hpp:rejection-calibration -->
```cpp
std::size_t calibration_samples() const
{
    return initial_.calibration_samples();
}

void calibrate(std::span<const double> deltas)
{
    initial_.calibrate(deltas);
    reset();
}
```

Calibration updates `initial_`, so later reheats use the estimated initial
temperature. The source checks both `temperature_policy<ReheatOnRejection>`
and `calibrating_temperature_policy<ReheatOnRejection>` with `static_assert`.

## Run and configure the variant

Select the policy as the template argument of the existing algorithm:

<!-- snippet: tutorial/annealing_main.cpp:custom-reheating -->
```cpp
using CustomAnnealing = runners::SimulatedAnnealing<ReheatOnRejection>;
auto custom = el::make_runner<CustomAnnealing>(
    {
        .temperature =
            {
                .descent =
                    {
                        .initial_temperature = 10.0,
                        .final_temperature = 0.1,
                        .cooling_rate = 0.9,
                        .samples_per_temperature = 20,
                    },
                .rejections_before_reheat = 10,
                .allowed_reheats = 2,
            },
        .max_evaluations = 5000,
    })
                  .with_solution_manager(sm)
                  .with_neighborhood(nhe);

std::mt19937_64 rng{42};
auto search = custom.bind(tsp);
const auto result = search.run(search.initial_solution(), rng);
```

The same variant can be registered in an app. Its nested schema provides
paths such as `runners.reheat.temperature.rejections_before_reheat` and
`runners.reheat.temperature.descent.cooling_rate`:

<!-- snippet: tutorial/annealing_main.cpp:reheating-app -->
```cpp
auto application =
    el::app("tsp-reheating")
        .with_solution_manager(sm)
        .with_neighborhood(nhe)
        .with_runner<CustomAnnealing>("reheat", custom.parameters());
el::Session session{application, tsp, 42};
const std::array changes{
    el::config::text_override{
        "runners.reheat.temperature.rejections_before_reheat",
        "20"},
    el::config::text_override{"runners.reheat.temperature.allowed_reheats", "3"},
};
const auto configured = session.configure(changes);
if (!configured)
    return 1;
session.use_initial_solution();
if (!session.run("reheat"))
    return 1;
```

The command line, TextUI and REST adapter can use this app as they do the
earlier TSP app. Give count parameters finite ranges when exporting them for
tuning ([chapter 12](12-tuning.md)).

Build and run the complete example:

```sh
cmake --build build/dev --target easylocal_tutorial_annealing
./build/dev/examples/tutorial/easylocal_tutorial_annealing
```

It runs both scheduled and rejection-triggered reheating. The five-city
instance makes the mechanics easy to inspect; use larger instances to assess
solution quality and speed. The tests exercise acceptance breaking a streak,
the reheat limit, reset, calibration, invalid parameters and evaluation-budget
handling, without relying on a lucky sequence of random moves.

## When to change the loop itself

A temperature policy sees only whether a proposal was accepted. A rule based
on iterations since the *best solution improved* needs more information.
For that change, write a runner on `search_run`, following the loop above and
[chapter 7](07-custom-runner.md). Likewise, change the acceptance policy if the
decision depends on candidate cost, current cost and temperature; its member
is `accept(candidate, current, temperature, rng)`.

Choose the smallest extension that has the information you need. A temperature
policy is enough for this reheating rule, and keeps the rest of SA's tested
implementation intact.

## See also

- [Runners](../reference/runners.md): temperature and acceptance contracts.
- [Observing and controlling a run](16-observing-and-controlling.md): recording
  temperature changes and applying common run limits.
