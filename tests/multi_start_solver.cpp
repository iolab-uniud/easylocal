#include "support/expect.hpp"

#include <easylocal/solvers.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace
{
struct Instance {};
struct Solution { std::uint64_t value{}; };
struct Move {};

struct MaximizingCost
{
    std::uint64_t value{};

    [[maybe_unused]] friend constexpr auto operator==(
        const MaximizingCost&,
        const MaximizingCost&) -> bool = default;

    friend constexpr auto operator<(
        const MaximizingCost lhs,
        const MaximizingCost rhs) noexcept -> bool
    {
        return lhs.value > rhs.value;
    }

    [[maybe_unused]] friend constexpr auto operator<=(
        const MaximizingCost lhs,
        const MaximizingCost rhs) noexcept -> bool
    {
        return lhs.value >= rhs.value;
    }
};

class MaximizingSM
{
public:
    using input_type = Instance;
    using solution_type = Solution;

    explicit MaximizingSM(const Instance& instance) : instance_{instance} {}
    auto input() const -> const Instance& { return instance_; }
    static auto is_valid(const Solution&) -> bool { return true; }
    template<class RNG>
    auto random_solution(RNG& rng) const -> Solution
    {
        return {rng() % 1000};
    }

private:
    const Instance& instance_;
};

struct MaximizingComponent
{
    static auto evaluate(const Solution& solution) -> MaximizingCost
    {
        return MaximizingCost{solution.value};
    }
};

template<class SM>
class EmptyNeighborhood
{
public:
    using input_type = typename SM::input_type;
    using solution_type = typename SM::solution_type;
    using move_type = Move;

    explicit EmptyNeighborhood(const SM& sm) : instance_{sm.input()} {}
    auto input() const -> const input_type& { return instance_; }
    [[nodiscard]] static auto is_valid(const solution_type&, const move_type&) noexcept -> bool { return true; }
    static void make_move(solution_type&, const move_type&) {}

private:
    const input_type& instance_;
};

struct Result
{
    Solution solution;
    MaximizingCost cost;
};

struct IdentityAlgorithm
{
    template<class Context>
    auto run(const Context&, typename Context::solution_type solution) const
        -> Result
    {
        return {solution, MaximizingCost{solution.value}};
    }
};

}

// Keeps the attempts of the run_context events, and whether one had a stage.
struct ContextTracer
{
    template<class Event>
    static constexpr bool observes =
        std::same_as<Event, easylocal::trace::event::run_context>;

    std::vector<std::size_t> attempts;
    bool staged{};

    void emit(const easylocal::trace::event::run_context& context)
    {
        attempts.push_back(context.attempt);
        staged = staged || !context.stage.empty() || context.stage_index != 0;
    }
};

int main()
{
    using namespace easylocal;
    bool ok = true;

    auto runner = Runner{IdentityAlgorithm{}}
        | (solution_manager<MaximizingSM>() | component<MaximizingComponent>())
        | neighborhood<EmptyNeighborhood<MaximizingSM>>();
    using RunnerType = decltype(runner);
    using Solver = solvers::MultiStart<RunnerType>;

    static_assert(!Solver::supports_initial);
    static_assert(Solver::supports_random);

    bool rejected_zero_starts = false;
    try
    {
        [[maybe_unused]] Solver invalid{
            runner,
            solvers::MultiStartParameters{.starts = 0},
            std::mt19937_64{42}};
    }
    catch (const std::invalid_argument&)
    {
        rejected_zero_starts = true;
    }
    ok &= expect(rejected_zero_starts, "zero starts are rejected");

    constexpr std::size_t starts = 5;
    constexpr std::uint64_t seed = 1234;
    auto solver =
        make_solver<solvers::MultiStart>(
            runner,
            solvers::MultiStartParameters{.starts = starts})
            .initialization(initialization::random)
            .seed(seed);

    std::mt19937_64 reference{seed};
    std::uint64_t expected_best = 0;
    for (std::size_t start = 0; start < starts; ++start)
        expected_best = std::max(expected_best, reference() % 1000);

    const Instance instance{};
    const auto result = solver.solve(instance);
    ok &= expect(
        result.solution.value == expected_best && result.cost.value == expected_best,
        "MultiStart retains the best result using cost semantics");

    std::uint64_t expected_second_best = 0;
    for (std::size_t start = 0; start < starts; ++start)
        expected_second_best = std::max(expected_second_best, reference() % 1000);
    const auto second = solver.solve(instance);
    ok &= expect(
        second.cost.value == expected_second_best,
        "MultiStart preserves the Solver-owned RNG stream across solve calls");

    // Each start is preceded by its run_context, outside any stage.
    ContextTracer tracer;
    static_cast<void>(solver.solve(instance, with(tracer)));
    ok &= expect(
        tracer.attempts == std::vector<std::size_t>{0, 1, 2, 3, 4} && !tracer.staged,
        "MultiStart emits the run_context of every start");

    return ok ? 0 : 1;
}
