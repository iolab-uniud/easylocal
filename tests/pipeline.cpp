// solvers::Pipeline: stages in sequence, their targets and attempts, the
// effort and report of each stage, cancellation and parameters.
#include <easylocal/app/app.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/solvers.hpp>
#include <easylocal/trace/events.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

struct Instance
{
};

struct Solution
{
    int hard{};
    int soft{};
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
        return {.hard = 5, .soft = 9};
    }

private:
    const Instance& instance_;
};

struct HardPart
{
    [[nodiscard]]
    static int evaluate(const Solution& solution)
    {
        return solution.hard;
    }
};

struct SoftPart
{
    [[nodiscard]]
    static int evaluate(const Solution& solution)
    {
        return solution.soft;
    }
};

struct Move
{
};

class Neighborhood
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    explicit Neighborhood(const SolutionManager& sm) : sm_{sm} {}

    [[nodiscard]]
    const Instance& input() const noexcept
    {
        return sm_.input();
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

// The result of the test algorithms: ten evaluations and one iteration a run.
template<class Cost>
struct Outcome
{
    Solution solution;
    Cost cost;
    std::size_t evaluations{10};
    std::size_t iterations{1};
    easylocal::termination_reason termination{easylocal::termination_reason::completed};
};

template<class Context>
auto outcome(const Context& context, const Solution& solution)
{
    return Outcome<typename Context::cost_type>{
        .solution = solution,
        .cost = context.evaluation().evaluate(solution).cost(),
    };
}

// The k-th run lowers the hard part by 2k: from 5, runs 1, 2 and 3 leave 3, 1
// and 0.
struct Countdown
{
    std::shared_ptr<int> runs = std::make_shared<int>(0);

    template<class Context>
    auto run(const Context& context, Solution solution) const
    {
        ++*runs;
        solution.hard = std::max(0, solution.hard - 2 * *runs);
        return outcome(context, solution);
    }
};

// Lowers the soft part by 4.
struct SoftDown
{
    template<class Context>
    auto run(const Context& context, Solution solution) const
    {
        solution.soft = std::max(0, solution.soft - 4);
        return outcome(context, solution);
    }
};

// Sets the soft part to 7, 3, 5, 6 in its successive runs.
struct Noisy
{
    std::shared_ptr<int> runs = std::make_shared<int>(0);

    template<class Context>
    auto run(const Context& context, Solution solution) const
    {
        static constexpr int values[] = {7, 3, 5, 6};
        solution.soft = values[(*runs)++ % 4];
        return outcome(context, solution);
    }
};

bool expect(const bool condition, const std::string_view message)
{
    if (!condition)
        std::cerr << "FAILED: " << message << '\n';
    return condition;
}

} // namespace

// The tests of the former TwoStage solver: two_stage() and the pipeline it
// stands for, with the same assertions.
namespace two_stage_cases
{

struct Instance
{
};

struct Solution
{
    int hard{};
    int soft{};
};

class SolutionManager
{
public:
    using input_type = Instance;
    using solution_type = Solution;

    explicit SolutionManager(const Instance& instance) : instance_{instance} {}

    [[nodiscard]] auto input() const noexcept -> const Instance&
    {
        return instance_;
    }
    [[nodiscard]] auto is_valid(const Solution&) const noexcept -> bool
    {
        return true;
    }
    [[nodiscard]] auto initial_solution() const -> Solution
    {
        return {5, 9};
    }

private:
    const Instance& instance_;
};

// A single component with a structured value: its hierarchical value is the
// cost.
struct HierarchicalValue
{
    [[nodiscard]] static auto evaluate(const Solution& solution)
        -> easylocal::cost::hierarchical<int, int>
    {
        return easylocal::cost::hierarchical{solution.hard, solution.soft};
    }
};

struct Move
{
};

class Neighborhood
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    explicit Neighborhood(const SolutionManager& sm) : sm_{sm} {}

    [[nodiscard]] auto input() const noexcept -> const Instance&
    {
        return sm_.input();
    }
    [[nodiscard]] static auto is_valid(const Solution&, const Move&) noexcept -> bool
    {
        return true;
    }
    void make_move(Solution&, const Move&) const noexcept {}

private:
    const SolutionManager& sm_;
};

template<class Cost>
struct Result
{
    Solution solution;
    Cost cost;
};

struct HardStage
{
    template<class Context>
    [[nodiscard]] auto run(const Context& context, Solution solution) const
    {
        static_assert(std::same_as<typename Context::cost_type, int>);
        solution.hard = 0;
        return Result<int>{solution, context.evaluation().evaluate(solution).cost()};
    }
};

struct FullStage
{
    template<class Context>
    [[nodiscard]] auto run(const Context& context, Solution solution) const
    {
        static_assert(easylocal::cost::hierarchical_type<typename Context::cost_type>);
        solution.soft = 1;
        return Result<typename Context::cost_type>{
            solution,
            context.evaluation().evaluate(solution).cost()};
    }
};

struct HardMove
{
};

class HardNeighborhood
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = HardMove;

    explicit HardNeighborhood(const SolutionManager& sm) : sm_{sm} {}

    [[nodiscard]] auto input() const noexcept -> const Instance&
    {
        return sm_.input();
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const Solution& solution, RNG&)
        -> std::optional<move_type>
    {
        return solution.hard > 0 ? std::optional<move_type>{move_type{}} : std::nullopt;
    }

    [[nodiscard]] static auto is_valid(const Solution&, const move_type&) noexcept -> bool
    {
        return true;
    }
    static void make_move(Solution& solution, const move_type&) noexcept
    {
        --solution.hard;
    }

private:
    const SolutionManager& sm_;
};

struct SoftMove
{
};

class SoftNeighborhood
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = SoftMove;

    explicit SoftNeighborhood(const SolutionManager& sm) : sm_{sm} {}

    [[nodiscard]] auto input() const noexcept -> const Instance&
    {
        return sm_.input();
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const Solution& solution, RNG&)
        -> std::optional<move_type>
    {
        return solution.hard == 0 && solution.soft > 0
            ? std::optional<move_type>{move_type{}}
            : std::nullopt;
    }

    [[nodiscard]] static auto is_valid(const Solution&, const move_type&) noexcept -> bool
    {
        return true;
    }
    static void make_move(Solution& solution, const move_type&) noexcept
    {
        --solution.soft;
    }

private:
    const SolutionManager& sm_;
};

// The hard and soft parts as two components, each with a delta under the
// same neighborhood: in the hard-only first stage the soft delta belongs to a
// component the projection leaves out, and is ignored.
struct HardPart
{
    [[nodiscard]] static auto evaluate(const Solution& solution) -> int
    {
        return solution.hard;
    }
};

struct SoftPart
{
    [[nodiscard]] static auto evaluate(const Solution& solution) -> int
    {
        return solution.soft;
    }
};

struct StepMove
{
    bool hard{};
};

class StepNeighborhood
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = StepMove;

    explicit StepNeighborhood(const SolutionManager& sm) : sm_{sm} {}

    [[nodiscard]] auto input() const noexcept -> const Instance&
    {
        return sm_.input();
    }

    // The hard part first, then the soft one.
    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const Solution& solution, RNG&)
        -> std::optional<move_type>
    {
        if (solution.hard > 0)
            return move_type{.hard = true};
        if (solution.soft > 0)
            return move_type{.hard = false};
        return std::nullopt;
    }

    [[nodiscard]] static auto is_valid(const Solution&, const move_type&) noexcept -> bool
    {
        return true;
    }
    static void make_move(Solution& solution, const move_type& move) noexcept
    {
        --(move.hard ? solution.hard : solution.soft);
    }

private:
    const SolutionManager& sm_;
};

struct HardPartDelta
{
    explicit HardPartDelta(const Instance&) {}
    [[nodiscard]] static auto delta_evaluate(const Solution&, const StepMove& move) -> int
    {
        return move.hard ? -1 : 0;
    }
};

struct SoftPartDelta
{
    explicit SoftPartDelta(const Instance&) {}
    [[nodiscard]] static auto delta_evaluate(const Solution&, const StepMove& move) -> int
    {
        return move.hard ? 0 : -1;
    }
};

// The hard and soft parts with co-located deltas (delta<C>()): the hard-only
// first stage must still reach both components.
struct ColocatedHardPart
{
    [[nodiscard]] static auto evaluate(const Solution& solution) -> int
    {
        return solution.hard;
    }
    [[nodiscard]] static auto delta_evaluate(const Solution&, const StepMove& move) -> int
    {
        return move.hard ? -1 : 0;
    }
};

struct ColocatedSoftPart
{
    [[nodiscard]] static auto evaluate(const Solution& solution) -> int
    {
        return solution.soft;
    }
    [[nodiscard]] static auto delta_evaluate(const Solution&, const StepMove& move) -> int
    {
        return move.hard ? 0 : -1;
    }
};

// Always proposes a move, also once the hard cost is zero: only the stage-1
// target stops a Simulated Annealing on it before its schedule ends.
class EndlessHardNeighborhood
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = HardMove;

    explicit EndlessHardNeighborhood(const SolutionManager& sm) : sm_{sm} {}

    [[nodiscard]] auto input() const noexcept -> const Instance&
    {
        return sm_.input();
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const Solution&, RNG&)
        -> std::optional<move_type>
    {
        return move_type{};
    }

    [[nodiscard]] static auto is_valid(const Solution&, const move_type&) noexcept -> bool
    {
        return true;
    }
    static void make_move(Solution& solution, const move_type&) noexcept
    {
        if (solution.hard > 0)
            --solution.hard;
    }

private:
    const SolutionManager& sm_;
};

// Counts the runs a tracer sees, whatever their cost type, and keeps the
// stage and attempt of each run_context.
struct RunCounter
{
    template<class Event>
    static constexpr bool observes =
        std::same_as<Event, easylocal::trace::event::run_context>;

    template<class Cost>
    static constexpr bool observes<easylocal::trace::event::run_started<Cost>> = true;

    std::size_t runs{};
    std::vector<std::string> contexts;

    template<class Event>
    void emit(const Event&) noexcept
    {
        ++runs;
    }

    void emit(const easylocal::trace::event::run_context& context)
    {
        contexts.push_back(
            std::string{context.stage} + ' ' + std::to_string(context.stage_index) + ' '
            + std::to_string(context.attempt));
    }
};

bool run()
{
    using namespace easylocal;

    const auto sm = solution_manager<SolutionManager>() | component<HierarchicalValue>();
    const auto nhe = neighborhood<Neighborhood>();

    auto first_runner = Runner{HardStage{}} | sm | nhe;
    auto second_runner = Runner{FullStage{}} | sm | nhe;

    using HardRunner = decltype(first_runner.with_hard_cost());
    using HardSM = typename HardRunner::solution_manager_type;
    static_assert(std::same_as<typename HardSM::cost_type, int>);
    static_assert(detail::hierarchical_solution_manager<
        typename decltype(second_runner)::solution_manager_type>);

    auto solver =
        solvers::two_stage(std::move(first_runner), std::move(second_runner))
            .initialization(initialization::initial)
            .seed(42);

    static_assert(decltype(solver)::supports_initial);

    auto shared_runner = Runner{HardStage{}} | sm | nhe;
    auto shared_solver =
        solvers::two_stage(shared_runner)
            .initialization(initialization::initial)
            .seed(42);
    static_assert(decltype(shared_solver)::supports_initial);
    // The builders return the pipeline by reference on an lvalue and by value
    // on a temporary, so that `auto&& p = pipeline(...).seed(1)` cannot dangle.
    using SharedPipeline = decltype(shared_solver);
    static_assert(
        std::same_as<decltype(std::declval<SharedPipeline&>().seed(1)), SharedPipeline&>);
    static_assert(
        std::same_as<decltype(std::declval<SharedPipeline>().seed(1)), SharedPipeline>);
    static_assert(std::same_as<
        decltype(std::declval<SharedPipeline&>().initialization(initialization::initial)),
        SharedPipeline&>);
    static_assert(std::same_as<
        decltype(std::declval<SharedPipeline>().initialization(initialization::initial)),
        SharedPipeline>);
    // two_stage(runner) is the pipeline of the same runner until feasible,
    // then on the whole cost.
    static_assert(std::same_as<
        decltype(shared_solver),
        decltype(solvers::pipeline(
            solvers::stage("first", shared_runner) & solvers::until_feasible(),
            solvers::stage("second", shared_runner)))>);

    const Instance instance{};
    const auto result = solver.solve(instance);

    bool ok = true;
    ok &= expect(result.solution.hard == 0, "stage 1 optimizes the hard branch");
    ok &= expect(result.solution.soft == 1, "stage 2 receives the stage-1 solution");
    ok &= expect(result.cost.hard() == 0, "final result keeps hierarchical hard cost");
    ok &= expect(result.cost.soft() == 1, "final result keeps hierarchical soft cost");

    auto hard_sa_runner =
        easylocal::make_runner<
            runners::SimulatedAnnealing<runners::temperature::FixedLength>>(
            {.temperature =
                    runners::temperature::FixedLengthParameters{
                        .initial_temperature = 2.0,
                        .final_temperature = 0.5,
                        .cooling_rate = 0.5,
                        .max_iterations = 32,
                    }})
        | sm | neighborhood<HardNeighborhood>();

    auto full_sa_runner =
        easylocal::make_runner<
            runners::SimulatedAnnealing<runners::temperature::FixedLength>>(
            {.temperature =
                    runners::temperature::FixedLengthParameters{
                        .initial_temperature = 2.0,
                        .final_temperature = 0.5,
                        .cooling_rate = 0.5,
                        .max_iterations = 32,
                    }})
        | sm | neighborhood<SoftNeighborhood>();

    auto sa_solver =
        solvers::two_stage(std::move(hard_sa_runner), std::move(full_sa_runner))
            .initialization(initialization::initial)
            .seed(17);

    const auto sa_result = sa_solver.solve(instance);
    ok &= expect(
        sa_result.solution.hard == 0 && sa_result.solution.soft == 0,
        "two-stage SA optimizes the numeric hard branch before the soft branch");
    ok &= expect(
        sa_result.cost.hard() == 0 && sa_result.cost.soft() == 0,
        "second-stage SA returns the full hierarchical cost");

    auto two_part_runner =
        easylocal::make_runner<
            runners::SimulatedAnnealing<runners::temperature::FixedLength>>(
            {.temperature =
                    runners::temperature::FixedLengthParameters{
                        .initial_temperature = 2.0,
                        .final_temperature = 0.5,
                        .cooling_rate = 0.5,
                        .max_iterations = 32,
                    }})
        | (solution_manager<SolutionManager>()
            | cost::hard_soft(component<HardPart>(), component<SoftPart>()))
        | (neighborhood<StepNeighborhood>() | delta<HardPart, HardPartDelta>()
            | delta<SoftPart, SoftPartDelta>());
    auto two_part_solver =
        solvers::two_stage(std::move(two_part_runner))
            .initialization(initialization::initial)
            .seed(5);
    const auto two_part_result = two_part_solver.solve(instance);
    ok &= expect(
        two_part_result.cost.hard() == 0 && two_part_result.cost.soft() == 0,
        "a soft delta is ignored by the hard-only stage and used by the full one");

    auto colocated_runner =
        easylocal::make_runner<
            runners::SimulatedAnnealing<runners::temperature::FixedLength>>(
            {.temperature =
                    runners::temperature::FixedLengthParameters{
                        .initial_temperature = 2.0,
                        .final_temperature = 0.5,
                        .cooling_rate = 0.5,
                        .max_iterations = 32,
                    }})
        | (solution_manager<SolutionManager>()
            | cost::hard_soft(
                component<ColocatedHardPart>(),
                component<ColocatedSoftPart>()))
        | (neighborhood<StepNeighborhood>() | delta<ColocatedHardPart>()
            | delta<ColocatedSoftPart>());
    auto colocated_solver =
        solvers::two_stage(std::move(colocated_runner))
            .initialization(initialization::initial)
            .seed(5);
    const auto colocated_result = colocated_solver.solve(instance);
    ok &= expect(
        colocated_result.cost.hard() == 0 && colocated_result.cost.soft() == 0,
        "two_stage() binds co-located deltas on a hard and a soft component");

    // Stage 1 stops as soon as the hard cost is zero, and the result reports
    // the effort of both stages.
    const auto long_schedule = runners::temperature::FixedLengthParameters{
        .initial_temperature = 2.0,
        .final_temperature = 0.5,
        .cooling_rate = 0.5,
        .max_iterations = 100000,
    };
    auto endless_hard_runner =
        easylocal::make_runner<
            runners::SimulatedAnnealing<runners::temperature::FixedLength>>(
            {.temperature = long_schedule})
        | sm | neighborhood<EndlessHardNeighborhood>();
    auto soft_runner =
        easylocal::make_runner<
            runners::SimulatedAnnealing<runners::temperature::FixedLength>>(
            {.temperature = long_schedule})
        | sm | neighborhood<SoftNeighborhood>();
    // The explicit pipeline that two_stage() stands for.
    auto stopping_solver =
        ((solvers::stage("first", std::move(endless_hard_runner))
             & solvers::until_feasible())
            | solvers::stage("second", std::move(soft_runner)))
            .initialization(initialization::initial)
            .seed(3);

    RunCounter counter;
    const run_control no_stop{};
    const auto stopped_at_feasible =
        stopping_solver.solve(instance, with(no_stop, counter));
    ok &= expect(
        stopped_at_feasible.solution.hard == 0 && stopped_at_feasible.solution.soft == 0,
        "two-stage SA reaches the optimum");
    // Stage 1: the initial evaluation and five decrements; stage 2: the
    // initial evaluation and nine decrements.
    ok &= expect(
        stopped_at_feasible.evaluations == 6 + 10,
        "stage 1 stops at a zero hard cost and the effort of both stages is reported");
    ok &= expect(counter.runs == 2, "the tracer sees both stages");
    ok &= expect(
        counter.contexts == std::vector<std::string>{"first 0 0", "second 1 0"},
        "each stage's run is preceded by its run_context, the hard stage included");

    std::stop_source stop;
    stop.request_stop();
    const run_control stopped{stop.get_token()};
    const auto cancelled = stopping_solver.solve(instance, with(stopped));
    ok &= expect(
        cancelled.termination == termination_reason::cancelled
            && cancelled.evaluations == 2,
        "after a cancellation the first and the last stage only evaluate their "
        "solution");

    // After a cancellation, or once the solve's budget is spent, the stages
    // between the first and the last are skipped, without binding their
    // runner: the last stage evaluates the solution once.
    auto three_stages =
        (solvers::stage(
             "first",
             easylocal::make_runner<
                 runners::SimulatedAnnealing<runners::temperature::FixedLength>>(
                 {.temperature = long_schedule})
                 | sm | neighborhood<EndlessHardNeighborhood>())
            & solvers::until_feasible())
        | solvers::stage(
            "middle",
            easylocal::make_runner<
                runners::SimulatedAnnealing<runners::temperature::FixedLength>>(
                {.temperature = long_schedule})
                | sm | neighborhood<SoftNeighborhood>())
        | solvers::stage(
            "last",
            easylocal::make_runner<
                runners::SimulatedAnnealing<runners::temperature::FixedLength>>(
                {.temperature = long_schedule})
                | sm | neighborhood<SoftNeighborhood>());
    three_stages.initialization(initialization::initial).seed(3);
    const auto skipped = three_stages.solve(instance, with(stopped));
    ok &= expect(
        skipped.evaluations == 2 && skipped.stages.size() == 3
            && skipped.stages[1].attempts == 0 && skipped.stages[1].evaluations == 0
            && skipped.stages[1].termination == termination_reason::cancelled
            && skipped.stages[2].evaluations == 1
            && skipped.termination == termination_reason::cancelled,
        "after a cancellation the middle stage is skipped");
    const auto spent = three_stages.solve(instance, max_evaluations(3));
    ok &= expect(
        spent.stages[0].evaluations == 3 && spent.stages[1].attempts == 0
            && spent.stages[1].termination
                == termination_reason::evaluation_budget_exhausted
            && spent.stages[2].attempts == 1
            && spent.termination == termination_reason::evaluation_budget_exhausted,
        "once the budget is spent the middle stage is skipped");

    return ok;
}

} // namespace two_stage_cases

int main()
{
    namespace el = easylocal;
    namespace solvers = easylocal::solvers;

    const auto sm = el::solution_manager<SolutionManager>()
        | el::cost::hard_soft(el::component<HardPart>(), el::component<SoftPart>());
    const auto nhe = el::neighborhood<Neighborhood>();
    const Instance instance{};
    bool ok = true;

    // Three stages: the first on the hard cost until it is zero, in up to five
    // attempts from new initial solutions; the others from its solution.
    const Countdown countdown;
    auto three =
        (solvers::stage("feasible", el::Runner{countdown} | sm | nhe)
            & solvers::until_feasible() & solvers::attempts(5))
        | solvers::stage("polish", el::Runner{SoftDown{}} | sm | nhe)
        | solvers::stage("finish", el::Runner{SoftDown{}} | sm | nhe);
    static_assert(decltype(three)::stage_count == 3);
    static_assert(
        std::same_as<std::remove_cvref_t<decltype(*three.stage<0>().target())>, int>);

    const auto result = three.initialization(el::initialization::initial).solve(instance);
    ok &= expect(*countdown.runs == 3, "the first stage stops at its target");
    ok &= expect(
        result.solution.hard == 0 && result.solution.soft == 1,
        "each stage starts from the solution of the previous one");
    ok &= expect(
        result.cost.hard() == 0 && result.cost.soft() == 1,
        "the result has the last stage's cost");
    ok &= expect(
        result.evaluations == 50 && result.iterations == 5,
        "the result has the effort of every stage and attempt");
    ok &= expect(result.stages.size() == 3, "one report per stage");
    ok &= expect(
        result.stages[0].name == "feasible" && result.stages[0].attempts == 3
            && result.stages[0].evaluations == 30 && result.stages[0].cost == "0",
        "the first stage's report");
    ok &= expect(
        result.stages[2].name == "finish" && result.stages[2].attempts == 1
            && result.stages[2].cost == "[0, 1]"
            && result.stages[2].termination == el::termination_reason::completed,
        "the last stage's report");

    // run() starts from a given solution, with the caller's RNG: the first
    // stage's attempts restart from it, so from 4 the second run reaches 0.
    const Countdown from_given;
    const auto given = solvers::pipeline(
        solvers::stage("feasible", el::Runner{from_given} | sm | nhe)
            & solvers::until_feasible() & solvers::attempts(5),
        solvers::stage("polish", el::Runner{SoftDown{}} | sm | nhe));
    std::mt19937_64 caller_rng{11};
    const auto from_solution =
        given.run(instance, Solution{.hard = 4, .soft = 6}, caller_rng);
    ok &= expect(
        *from_given.runs == 2 && from_solution.stages[0].attempts == 2
            && from_solution.solution.hard == 0 && from_solution.solution.soft == 2,
        "run() starts every attempt of the first stage from the given solution");

    // Without a target, a stage runs all its attempts and keeps the best.
    const Noisy noisy;
    auto best_of = solvers::pipeline(
        solvers::stage("noisy", el::Runner{noisy} | sm | nhe).with_attempts(4));
    const auto best = best_of.solve(instance);
    ok &= expect(
        *noisy.runs == 4 && best.solution.soft == 3 && best.stages[0].attempts == 4,
        "a stage without a target keeps its best attempt");

    // pipeline(a, b), a | b and pipeline(a).then(b) are the same pipeline.
    const auto shortcut = solvers::pipeline(
        solvers::stage("polish", el::Runner{SoftDown{}} | sm | nhe),
        solvers::stage("finish", el::Runner{SoftDown{}} | sm | nhe));
    const auto chained = solvers::stage("polish", el::Runner{SoftDown{}} | sm | nhe)
        | solvers::stage("finish", el::Runner{SoftDown{}} | sm | nhe);
    const auto spelled =
        solvers::pipeline(solvers::stage("polish", el::Runner{SoftDown{}} | sm | nhe))
            .then(solvers::stage("finish", el::Runner{SoftDown{}} | sm | nhe));
    static_assert(std::same_as<decltype(shortcut), decltype(chained)>);
    static_assert(std::same_as<decltype(shortcut), decltype(spelled)>);

    // A target in the stage's own cost ends its attempts: the hard part is 1
    // after the second run.
    const Countdown partial;
    auto to_one = solvers::pipeline(
        solvers::stage("partial", (el::Runner{partial} | sm | nhe).with_hard_cost())
        & solvers::target(1) & solvers::attempts(5));
    const auto reached =
        to_one.initialization(el::initialization::initial).solve(instance);
    ok &= expect(
        *partial.runs == 2 && reached.solution.hard == 1
            && reached.stages[0].attempts == 2,
        "a stage stops its attempts at its target");

    // A cancelled solve makes one attempt per stage, and the solve's own
    // target goes to the last stage.
    std::stop_source stop;
    stop.request_stop();
    const el::run_control stopped{stop.get_token()};
    const Countdown cancelled_countdown;
    auto cancellable = solvers::pipeline(
        solvers::stage("feasible", el::Runner{cancelled_countdown} | sm | nhe)
            .until_feasible()
            .with_attempts(5),
        solvers::stage("polish", el::Runner{SoftDown{}} | sm | nhe));
    const auto cancelled = cancellable.solve(
        instance,
        el::with(stopped).stop_at(el::cost::hierarchical{0, 0}));
    ok &= expect(
        *cancelled_countdown.runs == 1 && cancelled.stages[0].attempts == 1,
        "after a cancellation a stage makes no further attempt");

    // The parameters: each stage's own, and its runner's, under its name.
    auto annealing =
        el::make_runner<
            el::runners::SimulatedAnnealing<el::runners::temperature::FixedLength>>(
            {.temperature =
                    el::runners::temperature::FixedLengthParameters{
                        .initial_temperature = 2.0,
                        .final_temperature = 0.5,
                        .cooling_rate = 0.5,
                        .max_iterations = 32,
                    }})
        | sm | nhe;
    auto configurable = solvers::pipeline(
        solvers::stage("anneal", annealing) & solvers::attempts(2),
        solvers::stage("polish", el::Runner{SoftDown{}} | sm | nhe));
    const auto parameters = configurable.configuration();
    std::vector<std::string> paths;
    for (const auto& parameter : parameters.parameters())
        paths.push_back(parameter.path);
    const auto has = [&paths](const std::string_view path) {
        return std::ranges::find(paths, path) != paths.end();
    };
    ok &= expect(
        has("anneal.attempts") && has("polish.attempts"),
        "every stage has its attempts");
    ok &= expect(
        std::ranges::any_of(
            paths,
            [](const std::string& path) { return path.starts_with("anneal.search."); }),
        "a stage's runner has its parameters under the stage's name");
    const auto attempts_override = el::config::apply_overrides(
        parameters,
        std::vector<el::config::text_override>{{"anneal.attempts", "3"}});
    ok &= expect(
        attempts_override && configurable.stage<0>().parameters().attempts == 3,
        "the attempts are set through the parameters");

    // An app over services that derive from no EasyLocal base composes with
    // |, and runs its pipeline by name.
    auto plain_app = el::app("plain") | sm | nhe
        | el::pipeline(
            "polish",
            solvers::stage("polish", el::Runner{SoftDown{}} | sm | nhe));
    std::mt19937_64 app_rng{2};
    const auto polished =
        plain_app.run("polish", instance, Solution{.hard = 0, .soft = 9}, app_rng);
    ok &= expect(
        polished.has_value() && polished->solution.soft == 5,
        "an app of plain services is composed with | and runs its pipeline");

    // Stage names are distinct and not empty; a stage has at least one attempt.
    auto twins = solvers::pipeline(
        solvers::stage("same", el::Runner{SoftDown{}} | sm | nhe),
        solvers::stage("same", el::Runner{SoftDown{}} | sm | nhe));
    bool rejected = false;
    try
    {
        static_cast<void>(twins.configuration());
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    ok &= expect(rejected, "two stages with the same name are rejected");
    rejected = false;
    try
    {
        static_cast<void>(
            solvers::stage("none", el::Runner{SoftDown{}} | sm | nhe)
            & solvers::attempts(0));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    ok &= expect(rejected, "a stage without attempts is rejected");

    ok &= two_stage_cases::run();

    return ok ? 0 : 1;
}
