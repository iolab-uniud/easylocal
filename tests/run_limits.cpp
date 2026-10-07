// The limits a caller gives a run, a solve or a pipeline stage: a time limit,
// easylocal::timeout(d) with a std::chrono duration or a number of seconds,
// and an evaluation budget, easylocal::max_evaluations(n), alone or with the
// other run options.
#include "support/expect.hpp"

#include <easylocal/cost.hpp>
#include <easylocal/runners/hill_climbing.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/solvers/local_search.hpp>
#include <easylocal/solvers/multi_start.hpp>
#include <easylocal/solvers/pipeline.hpp>
#include <easylocal/trace/memory_recorder.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace
{

struct Instance
{
};

struct Solution
{
    int value{};
};

class SolutionManager
{
public:
    using input_type = Instance;
    using solution_type = Solution;

    explicit SolutionManager(const Instance& instance) : instance_{instance} {}

    [[nodiscard]]
    const Instance& input() const noexcept
    {
        return instance_;
    }

    [[nodiscard]]
    static bool is_valid(const Solution&) noexcept
    {
        return true;
    }

    [[nodiscard]]
    static Solution initial_solution()
    {
        return {.value = 7};
    }

private:
    const Instance& instance_;
};

struct Value
{
    [[nodiscard]]
    static int evaluate(const Solution& solution)
    {
        return solution.value;
    }
};

struct Move
{
};

// Always proposes a move that changes nothing: a hill climbing on it never
// improves and, without a limit, never ends.
class EndlessNeighborhood
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    explicit EndlessNeighborhood(const SolutionManager& sm) : sm_{sm} {}

    [[nodiscard]]
    const Instance& input() const noexcept
    {
        return sm_.input();
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    static std::optional<Move> random_move(const Solution&, RNG&)
    {
        return Move{};
    }

    [[nodiscard]]
    static bool is_valid(const Solution&, const Move&) noexcept
    {
        return true;
    }

    static void make_move(Solution&, const Move&) noexcept {}

private:
    const SolutionManager& sm_;
};

} // namespace

int main()
{
    namespace el = easylocal;
    using namespace std::chrono_literals;
    using clock = std::chrono::steady_clock;

    const Instance instance{};
    auto runner =
        el::make_runner<el::runners::HillClimbing>({.max_idle_iterations = el::unlimited})
        | (el::solution_manager<SolutionManager>() | el::component<Value>())
        | el::neighborhood<EndlessNeighborhood>();
    auto bound = runner.bind(instance);
    std::mt19937_64 rng{1};
    bool ok = true;

    // A run that would never end stops at its time limit.
    const auto started = clock::now();
    const auto limited = bound.run(bound.initial_solution(), rng, el::timeout(50ms));
    const auto elapsed = clock::now() - started;
    ok &= expect(
        limited.termination == el::termination_reason::time_limit_reached,
        "a run stops at its time limit");
    ok &= expect(elapsed >= 50ms && elapsed < 5s, "a run lasts about its time limit");
    ok &= expect(limited.evaluations > 1, "a run works until its time limit");
    ok &= expect(
        el::to_string(limited.termination) == "time limit reached",
        "the termination has a readable name");

    // No time at all: the run stops at its first check, after the initial
    // evaluation.
    const auto immediate = bound.run(bound.initial_solution(), rng, el::timeout(0s));
    ok &= expect(
        immediate.termination == el::termination_reason::time_limit_reached
            && immediate.evaluations == 1,
        "a run without time stops at once");

    // A number of seconds is the same limit.
    const auto in_seconds = bound.run(bound.initial_solution(), rng, el::timeout(0.0));
    ok &= expect(
        in_seconds.termination == el::termination_reason::time_limit_reached
            && in_seconds.evaluations == 1,
        "a time limit in seconds");
    ok &= expect(
        el::timeout(2.5).time_limit == el::timeout(2500ms).time_limit,
        "seconds and a duration give the same limit");

    // With the other options: each one is kept.
    std::stop_source stop;
    const el::run_control control{stop.get_token()};
    const auto combined = el::with(control).timeout(30s).stop_at(7);
    ok &= expect(
        combined.control == &control && combined.target == 7
            && combined.time_limit == el::timeout(30s).time_limit,
        "timeout and stop_at combine");
    const auto reached = bound.run(bound.initial_solution(), rng, combined);
    ok &= expect(
        reached.termination == el::termination_reason::target_reached,
        "the target ends a run before its time limit");
    const auto reversed = el::with(control).stop_at(7).timeout(30s);
    ok &= expect(
        reversed.target == 7 && reversed.time_limit == combined.time_limit,
        "stop_at keeps a time limit set before");

    // A limit beyond what the clock counts is no limit; a negative one is an
    // error.
    ok &= expect(
        el::timeout(std::chrono::hours::max()).time_limit == clock::duration::max(),
        "a limit beyond the clock is the largest one");
    bool rejected = false;
    try
    {
        static_cast<void>(el::timeout(-1.0));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    ok &= expect(rejected, "a negative time limit is rejected");
    rejected = false;
    try
    {
        static_cast<void>(el::timeout(std::numeric_limits<double>::quiet_NaN()));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    ok &= expect(rejected, "a time limit that is not a number is rejected");
    rejected = false;
    try
    {
        static_cast<void>(el::timeout(
            std::chrono::duration<double>{std::numeric_limits<double>::quiet_NaN()}));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    ok &= expect(rejected, "a duration that is not a number is rejected");

    // A LocalSearch gives its time limit to its one run.
    namespace solvers = el::solvers;
    auto single =
        el::make_solver<solvers::LocalSearch>(runner)
            .initialization(el::initialization::initial)
            .seed(4);
    const auto searched = single.solve(instance, el::timeout(20ms));
    ok &= expect(
        searched.termination == el::termination_reason::time_limit_reached,
        "a LocalSearch stops at its time limit");

    // An evaluation budget stops a run that would never end, at exactly that
    // many evaluations, the initial one included.
    const auto budgeted =
        bound.run(bound.initial_solution(), rng, el::max_evaluations(10));
    ok &= expect(
        budgeted.termination == el::termination_reason::evaluation_budget_exhausted
            && budgeted.evaluations == 10,
        "a run stops at its evaluation budget");

    // The caller's budget tightens the runner's own, never widens it.
    auto frugal =
        el::make_runner<el::runners::HillClimbing>(
            {.max_idle_iterations = el::unlimited, .max_evaluations = 5})
        | (el::solution_manager<SolutionManager>() | el::component<Value>())
        | el::neighborhood<EndlessNeighborhood>();
    auto frugal_bound = frugal.bind(instance);
    ok &= expect(
        frugal_bound.run(frugal_bound.initial_solution(), rng, el::max_evaluations(10))
                .evaluations
            == 5,
        "a larger caller budget leaves the runner's own");
    ok &= expect(
        frugal_bound.run(frugal_bound.initial_solution(), rng, el::max_evaluations(3))
                .evaluations
            == 3,
        "a smaller caller budget tightens the runner's own");

    // Every option keeps the others.
    const auto all = el::with(control).timeout(30s).max_evaluations(7).stop_at(100);
    ok &= expect(
        all.control == &control && all.target == 100 && all.evaluation_limit == 7
            && all.time_limit == el::timeout(30s).time_limit,
        "max_evaluations combines with timeout and stop_at");
    ok &= expect(
        el::with(control).max_evaluations(7).timeout(1s).evaluation_limit == 7,
        "timeout keeps an evaluation budget set before");
    ok &= expect(
        el::with(control).evaluation_limit.is_unlimited()
            && !el::with(control).time_limit,
        "run options have no evaluation budget and no time limit by default");

    // A solve's time limit bounds all its runs: a MultiStart of endless starts
    // stops at it.
    auto restarts =
        el::make_solver<solvers::MultiStart>(
            runner,
            solvers::MultiStartParameters{.starts = 1000})
            .initialization(el::initialization::initial)
            .seed(3);
    const auto multi_started = clock::now();
    const auto restarted = restarts.solve(instance, el::timeout(60ms));
    const auto multi_elapsed = clock::now() - multi_started;
    ok &= expect(
        restarted.termination == el::termination_reason::time_limit_reached
            && multi_elapsed >= 60ms && multi_elapsed < 5s,
        "a MultiStart stops at the solve's time limit");

    // A stage's own limit ends that stage; the next one has what is left of
    // the solve's limit, wide enough to leave it time on a busy machine.
    auto timed = solvers::pipeline(
        solvers::stage("first", runner) & solvers::timeout(100ms),
        solvers::stage("second", runner));
    const auto pipeline_started = clock::now();
    const auto staged =
        timed.initialization(el::initialization::initial)
            .solve(instance, el::timeout(400ms));
    const auto pipeline_elapsed = clock::now() - pipeline_started;
    ok &= expect(
        staged.stages.size() == 2
            && staged.stages[0].termination == el::termination_reason::time_limit_reached
            && staged.stages[1].termination == el::termination_reason::time_limit_reached
            && staged.termination == el::termination_reason::time_limit_reached,
        "both stages stop at a time limit");
    ok &= expect(
        pipeline_elapsed >= 400ms && pipeline_elapsed < 5s,
        "a pipeline lasts about the solve's time limit");
    ok &= expect(
        staged.stages[0].evaluations < staged.evaluations,
        "the first stage stops before the solve's limit");

    // The spellings of a stage's limit, and the parameter that holds it.
    static_assert(std::is_same_v<
        decltype(solvers::stage("s", runner).with_timeout(0.03)),
        decltype(solvers::stage("s", runner) & solvers::timeout(30ms))>);
    ok &= expect(
        (solvers::stage("s", runner) & el::timeout(1.5)).parameters().timeout == 1.5
            && solvers::stage("s", runner).with_timeout(250ms).parameters().timeout
                == 0.25,
        "a stage's time limit in seconds");
    auto parameters = timed.configuration();
    std::vector<std::string> paths;
    for (const auto& parameter : parameters.parameters())
        paths.push_back(parameter.path);
    ok &= expect(
        std::ranges::find(paths, std::string{"second.timeout"}) != paths.end(),
        "a stage's time limit is a parameter");
    const auto set = el::config::apply_overrides(
        parameters,
        std::vector<el::config::text_override>{{"second.timeout", "0.5"}});
    ok &= expect(
        set && timed.stage<1>().parameters().timeout == 0.5,
        "a stage's time limit is set through the parameters");
    const auto invalid = el::config::apply_overrides(
        parameters,
        std::vector<el::config::text_override>{{"second.timeout", "-1"}});
    ok &= expect(!invalid, "a negative stage time limit is rejected");

    // The trace says why each run ended.
    el::trace::memory_recorder<int> timed_trace;
    static_cast<void>(
        bound.run(bound.initial_solution(), rng, el::with(timed_trace).timeout(0s)));
    el::trace::memory_recorder<int> budget_trace;
    static_cast<void>(bound.run(
        bound.initial_solution(),
        rng,
        el::with(budget_trace).max_evaluations(3)));
    using finished = el::trace::memory_recorder<int>::run_finished_record;
    const auto* const timed_end = std::get_if<finished>(&timed_trace.records().back());
    const auto* const budget_end = std::get_if<finished>(&budget_trace.records().back());
    ok &= expect(
        timed_end != nullptr
            && timed_end->termination == el::termination_reason::time_limit_reached,
        "the trace records a run stopped by its time limit");
    ok &= expect(
        budget_end != nullptr
            && budget_end->termination
                == el::termination_reason::evaluation_budget_exhausted,
        "the trace records a run stopped by its evaluation budget");

    // A solve's evaluation budget is shared by its runs: MultiStart's starts
    // and a pipeline's stages together make at most that many.
    const auto shared = restarts.solve(instance, el::max_evaluations(25));
    ok &= expect(
        shared.evaluations == 25
            && shared.termination == el::termination_reason::evaluation_budget_exhausted,
        "a MultiStart shares the solve's evaluation budget");
    // The budget may run out during the last start: the solve still says so.
    auto single_start =
        el::make_solver<solvers::MultiStart>(
            runner,
            solvers::MultiStartParameters{.starts = 1})
            .initialization(el::initialization::initial)
            .seed(3);
    const auto last_start = single_start.solve(instance, el::max_evaluations(5));
    ok &= expect(
        last_start.evaluations == 5
            && last_start.termination
                == el::termination_reason::evaluation_budget_exhausted,
        "a MultiStart whose last start spends the budget says so");

    // A library recorder sees each start of a MultiStart as its run_context,
    // then its run, from run_started to run_finished.
    {
        using recorder_type = el::trace::memory_recorder<int>;
        auto three_starts =
            el::make_solver<solvers::MultiStart>(
                frugal,
                solvers::MultiStartParameters{.starts = 3})
                .initialization(el::initialization::initial)
                .seed(3);
        recorder_type recorder;
        static_cast<void>(three_starts.solve(instance, el::with(recorder)));
        std::vector<std::size_t> attempts;
        std::vector<std::size_t> evaluations;
        bool in_order = true;
        const auto& records = recorder.records();
        for (std::size_t index = 0; index < records.size(); ++index)
        {
            if (const auto* context =
                    std::get_if<recorder_type::run_context_record>(&records[index]))
            {
                attempts.push_back(context->attempt);
                in_order = in_order && index + 1 < records.size()
                    && std::holds_alternative<recorder_type::run_started_record>(
                        records[index + 1]);
            }
            else if (const auto* end =
                         std::get_if<recorder_type::run_finished_record>(&records[index]))
            {
                evaluations.push_back(end->evaluations);
            }
        }
        ok &= expect(
            attempts == std::vector<std::size_t>{0, 1, 2} && in_order
                && evaluations == std::vector<std::size_t>{5, 5, 5},
            "a memory recorder records each start of a MultiStart after its run_context");
    }
    auto budgeted_pipeline = solvers::pipeline(
        solvers::stage("first", runner) & solvers::max_evaluations(10),
        solvers::stage("second", runner));
    const auto budgeted_stages =
        budgeted_pipeline.initialization(el::initialization::initial)
            .solve(instance, el::max_evaluations(40));
    ok &= expect(
        budgeted_stages.stages[0].evaluations == 10
            && budgeted_stages.stages[1].evaluations == 30
            && budgeted_stages.evaluations == 40,
        "a stage's own budget ends it, and the next one has what is left");
    // The termination of a stage, and of the solve, is why it stopped, not why
    // its best attempt did: the first attempt ends idle, the second runs out
    // of the budget, and the third never starts.
    auto idle_runner =
        el::make_runner<el::runners::HillClimbing>({.max_idle_iterations = 10})
        | (el::solution_manager<SolutionManager>() | el::component<Value>())
        | el::neighborhood<EndlessNeighborhood>();
    auto attempted =
        solvers::pipeline(solvers::stage("only", idle_runner) & solvers::attempts(3));
    const auto attempted_result =
        attempted.initialization(el::initialization::initial)
            .solve(instance, el::max_evaluations(15));
    ok &= expect(
        attempted_result.stages[0].attempts == 2
            && attempted_result.stages[0].termination
                == el::termination_reason::evaluation_budget_exhausted
            && attempted_result.termination
                == el::termination_reason::evaluation_budget_exhausted,
        "a stage stopped by the budget in its second attempt says so");
    ok &= expect(
        solvers::stage("s", runner).with_max_evaluations(7).parameters().max_evaluations
            == 7,
        "with_max_evaluations sets the stage's budget");
    auto budget_parameters = budgeted_pipeline.configuration();
    const auto budget_set = el::config::apply_overrides(
        budget_parameters,
        std::vector<el::config::text_override>{{"second.max_evaluations", "5"}});
    ok &= expect(
        budget_set && budgeted_pipeline.stage<1>().parameters().max_evaluations == 5,
        "a stage's budget is set through the parameters");

    return ok ? 0 : 1;
}
