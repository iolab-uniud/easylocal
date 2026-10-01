#include <easylocal/cost.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/solvers.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/trace/events.hpp>

#include <concepts>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <stop_token>
#include <string_view>
#include <utility>

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

    [[nodiscard]] auto input() const noexcept -> const Instance& { return instance_; }
    [[nodiscard]] auto is_valid(const Solution&) const noexcept -> bool { return true; }
    [[nodiscard]] auto initial_solution() const -> Solution { return {5, 9}; }
private:
    const Instance& instance_;
};

// A single component with a structured value: the implicit identity aggregator
// makes its hierarchical value the cost.
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

    [[nodiscard]] auto input() const noexcept -> const Instance& { return sm_.input(); }
    [[nodiscard]] static auto is_valid(const Solution&, const Move&) noexcept -> bool { return true; }
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
        static_assert(easylocal::cost::hierarchical_type<
            typename Context::cost_type>);
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

    [[nodiscard]] auto input() const noexcept -> const Instance& { return sm_.input(); }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const Solution& solution, RNG&)
        -> std::optional<move_type>
    {
        return solution.hard > 0
            ? std::optional<move_type>{move_type{}}
            : std::nullopt;
    }

    [[nodiscard]] static auto is_valid(const Solution&, const move_type&) noexcept -> bool { return true; }
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

    [[nodiscard]] auto input() const noexcept -> const Instance& { return sm_.input(); }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const Solution& solution, RNG&)
        -> std::optional<move_type>
    {
        return solution.hard == 0 && solution.soft > 0
            ? std::optional<move_type>{move_type{}}
            : std::nullopt;
    }

    [[nodiscard]] static auto is_valid(const Solution&, const move_type&) noexcept -> bool { return true; }
    static void make_move(Solution& solution, const move_type&) noexcept
    {
        --solution.soft;
    }

private:
    const SolutionManager& sm_;
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

    [[nodiscard]] auto input() const noexcept -> const Instance& { return sm_.input(); }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const Solution&, RNG&) -> std::optional<move_type>
    {
        return move_type{};
    }

    [[nodiscard]] static auto is_valid(const Solution&, const move_type&) noexcept -> bool { return true; }
    static void make_move(Solution& solution, const move_type&) noexcept
    {
        if (solution.hard > 0)
        {
            --solution.hard;
        }
    }

private:
    const SolutionManager& sm_;
};

// Counts the runs a tracer sees, whatever their cost type.
struct RunCounter
{
    template<class Event>
    static constexpr bool observes = false;

    template<class Cost>
    static constexpr bool observes<easylocal::trace::event::run_started<Cost>> = true;

    std::size_t runs{};

    template<class Event>
    void emit(const Event&) noexcept
    {
        ++runs;
    }
};

auto expect(bool condition, std::string_view message) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
    return true;
}

} // namespace

int main()
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

    auto solver = make_solver<solvers::TwoStage>(
        std::move(first_runner),
        std::move(second_runner),
        solvers::TwoStageConfig<initialization::Initial>{
            .initialization = initialization::initial,
            .seed = 42,
        });

    static_assert(decltype(solver)::supports_initial);

    auto shared_runner = Runner{HardStage{}} | sm | nhe;
    auto shared_solver = make_solver<solvers::TwoStage>(
        std::move(shared_runner),
        solvers::TwoStageConfig<initialization::Initial>{
            .initialization = initialization::initial,
            .seed = 42,
        });
    static_assert(decltype(shared_solver)::supports_initial);

    const Instance instance{};
    const auto result = solver.solve(instance);

    bool ok = true;
    ok &= expect(result.solution.hard == 0, "stage 1 optimizes the hard branch");
    ok &= expect(result.solution.soft == 1, "stage 2 receives the stage-1 solution");
    ok &= expect(result.cost.hard() == 0, "final result keeps hierarchical hard cost");
    ok &= expect(result.cost.soft() == 1, "final result keeps hierarchical soft cost");

    auto hard_sa_runner =
        Runner{runners::SimulatedAnnealing{
            runners::temperature::FixedLength{
                runners::temperature::FixedLengthParameters{
                    .initial_temperature = 2.0,
                    .final_temperature = 0.5,
                    .cooling_rate = 0.5,
                    .max_iterations = 32,
                }}}}
        | sm
        | neighborhood<HardNeighborhood>();

    auto full_sa_runner =
        Runner{runners::SimulatedAnnealing{
            runners::temperature::FixedLength{
                runners::temperature::FixedLengthParameters{
                    .initial_temperature = 2.0,
                    .final_temperature = 0.5,
                    .cooling_rate = 0.5,
                    .max_iterations = 32,
                }}}}
        | sm
        | neighborhood<SoftNeighborhood>();

    auto sa_solver = make_solver<solvers::TwoStage>(
        std::move(hard_sa_runner),
        std::move(full_sa_runner),
        solvers::TwoStageConfig<initialization::Initial>{
            .initialization = initialization::initial,
            .seed = 17,
        });

    const auto sa_result = sa_solver.solve(instance);
    ok &= expect(
        sa_result.solution.hard == 0 && sa_result.solution.soft == 0,
        "two-stage SA optimizes the numeric hard branch before the soft branch");
    ok &= expect(
        sa_result.cost.hard() == 0 && sa_result.cost.soft() == 0,
        "second-stage SA returns the full hierarchical cost");

    // Stage 1 stops as soon as the hard cost is zero, and the result reports
    // the effort of both stages.
    const auto long_schedule = runners::temperature::FixedLengthParameters{
        .initial_temperature = 2.0,
        .final_temperature = 0.5,
        .cooling_rate = 0.5,
        .max_iterations = 100000,
    };
    auto endless_hard_runner =
        Runner{runners::SimulatedAnnealing{runners::temperature::FixedLength{long_schedule}}}
        | sm
        | neighborhood<EndlessHardNeighborhood>();
    auto soft_runner =
        Runner{runners::SimulatedAnnealing{runners::temperature::FixedLength{long_schedule}}}
        | sm
        | neighborhood<SoftNeighborhood>();
    auto stopping_solver = make_solver<solvers::TwoStage>(
        std::move(endless_hard_runner),
        std::move(soft_runner),
        solvers::TwoStageConfig<initialization::Initial>{
            .initialization = initialization::initial,
            .seed = 3,
        });

    RunCounter counter;
    const run_control no_stop{};
    const auto stopped_at_feasible = stopping_solver.solve(instance, with(no_stop, counter));
    ok &= expect(
        stopped_at_feasible.solution.hard == 0 && stopped_at_feasible.solution.soft == 0,
        "two-stage SA reaches the optimum");
    // Stage 1: the initial evaluation and five decrements; stage 2: the
    // initial evaluation and nine decrements.
    ok &= expect(
        stopped_at_feasible.evaluations == 6 + 10,
        "stage 1 stops at a zero hard cost and the effort of both stages is reported");
    ok &= expect(counter.runs == 2, "the tracer sees both stages");

    std::stop_source stop;
    stop.request_stop();
    const run_control stopped{stop.get_token()};
    const auto cancelled = stopping_solver.solve(instance, with(stopped));
    ok &= expect(
        cancelled.termination == termination_reason::cancelled && cancelled.evaluations == 2,
        "after a cancellation each stage only evaluates its solution");

    return ok ? 0 : 1;
}
