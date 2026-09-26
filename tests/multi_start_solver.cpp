#include <easylocal/solver.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string_view>

namespace
{
struct Instance {};
struct Solution { std::uint64_t value{}; };
struct Move {};

struct MaximizingCost
{
    std::uint64_t value{};

    friend constexpr auto operator==(
        const MaximizingCost&,
        const MaximizingCost&) -> bool = default;

    friend constexpr auto operator<(
        const MaximizingCost lhs,
        const MaximizingCost rhs) noexcept -> bool
    {
        return lhs.value > rhs.value;
    }

    friend constexpr auto operator<=(
        const MaximizingCost lhs,
        const MaximizingCost rhs) noexcept -> bool
    {
        return lhs.value >= rhs.value;
    }
};

class MaximizingSM
{
public:
    using instance_type = Instance;
    using solution_type = Solution;
    using cost_type = MaximizingCost;

    explicit MaximizingSM(const Instance& instance) : instance_{instance} {}
    auto instance() const -> const Instance& { return instance_; }
    static auto is_valid(const Solution&) -> bool { return true; }
    static auto evaluate(const Solution& solution) -> cost_type
    {
        return MaximizingCost{solution.value};
    }
    template<class RNG>
    auto random_solution(RNG& rng) const -> Solution
    {
        return {rng() % 1000};
    }

private:
    const Instance& instance_;
};

template<class SM>
class EmptyNeighborhood
{
public:
    using instance_type = typename SM::instance_type;
    using solution_type = typename SM::solution_type;
    using move_type = Move;

    explicit EmptyNeighborhood(const SM& sm) : instance_{sm.instance()} {}
    auto instance() const -> const instance_type& { return instance_; }
    [[nodiscard]] static auto is_valid(const solution_type&, const move_type&) noexcept -> bool { return true; }
    static void make_move(solution_type&, const move_type&) {}

private:
    const instance_type& instance_;
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

bool expect(const bool condition, const std::string_view message)
{
    if (!condition) std::cerr << "FAILED: " << message << '\n';
    return condition;
}
}

int main()
{
    using namespace easylocal;
    bool ok = true;

    auto runner = Runner{IdentityAlgorithm{}}
        | solution_manager<MaximizingSM>()
        | neighborhood<EmptyNeighborhood<MaximizingSM>>();
    using RunnerType = decltype(runner);
    using Solver = MultiStartSolver<RunnerType>;

    static_assert(!Solver::supports_initial);
    static_assert(Solver::supports_random);

    bool rejected_zero_starts = false;
    try
    {
        [[maybe_unused]] Solver invalid{
            runner,
            MultiStartParameters{.starts = 0},
            initialization::random,
            std::mt19937_64{42}};
    }
    catch (const std::invalid_argument&)
    {
        rejected_zero_starts = true;
    }
    ok &= expect(rejected_zero_starts, "zero starts are rejected");

    constexpr std::size_t starts = 5;
    constexpr std::uint64_t seed = 1234;
    auto solver = make_multi_start_solver(
        runner,
        MultiStartParameters{.starts = starts},
        initialization::random,
        seed);

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

    return ok ? 0 : 1;
}
