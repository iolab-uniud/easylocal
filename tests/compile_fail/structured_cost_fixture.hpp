#pragma once

// A problem whose cost is a structure ordered by < with no numeric difference:
// neither arithmetic, nor with a cost::delta, nor a cost::pareto. The runners
// that need one of them reject it with a message.

#include <easylocal/runners/runner.hpp>
#include <easylocal/utils/generator.hpp>

#include <optional>
#include <random>

namespace structured_cost_fixture
{

struct Instance
{
};
struct Solution
{
    int value{};
};
struct Move
{
    int delta{};
};
struct StructuredCost
{
    int hard{};
    int soft{};

    friend bool operator<(const StructuredCost& lhs, const StructuredCost& rhs) noexcept
    {
        return lhs.hard < rhs.hard;
    }
};

class SolutionManager
{
public:
    using input_type = Instance;
    using solution_type = Solution;

    explicit SolutionManager(const Instance& instance) noexcept : instance_{instance} {}

    [[nodiscard]] const Instance& input() const noexcept
    {
        return instance_;
    }
    [[nodiscard]] static bool is_valid(const Solution&) noexcept
    {
        return true;
    }

private:
    const Instance& instance_;
};

struct StructuredValue
{
    [[nodiscard]] static StructuredCost evaluate(const Solution& solution) noexcept
    {
        return {.hard = solution.value, .soft = 0};
    }
};

class Neighborhood
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    explicit Neighborhood(const SolutionManager& manager) noexcept
        : instance_{manager.input()}
    {
    }

    [[nodiscard]] const Instance& input() const noexcept
    {
        return instance_;
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static std::optional<Move> random_move(const Solution&, RNG&)
    {
        return Move{.delta = -1};
    }

    [[nodiscard]] static easylocal::generator<Move> moves(const Solution&)
    {
        co_yield Move{.delta = -1};
    }

    [[nodiscard]] static bool is_valid(const Solution&, const Move&) noexcept
    {
        return true;
    }
    static void make_move(Solution& solution, const Move& move) noexcept
    {
        solution.value += move.delta;
    }

private:
    const Instance& instance_;
};

// The runner of Algorithm on the structured cost, run once.
template<class Algorithm>
void run_on_structured_cost(typename Algorithm::parameters_type parameters = {})
{
    using namespace easylocal;
    const Instance instance;
    auto runner = make_runner<Algorithm>(parameters)
        | (solution_manager<SolutionManager>() | component<StructuredValue>())
        | neighborhood<Neighborhood>();
    std::mt19937 rng{1U};
    (void)runner.bind(instance).run(Solution{}, rng);
}

} // namespace structured_cost_fixture
