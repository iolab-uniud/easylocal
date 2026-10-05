#include <easylocal/runners/best_improvement.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/hill_climbing.hpp>
#include <easylocal/runners/runner.hpp>

#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <iostream>
#include <optional>
#include <random>
#include <string_view>
#include <utility>

namespace
{

using easylocal::runners::BestImprovement;
using easylocal::runners::BestImprovementParameters;
using easylocal::runners::FirstImprovement;
using easylocal::runners::FirstImprovementParameters;

struct Instance
{
};

struct Solution
{
    int score{};
};

struct Move
{
    int delta{};
};

struct ScoreValue
{
    int value{};
};

struct OpaqueCost
{
    int value{};
};

static_assert(!std::equality_comparable<OpaqueCost>);
static_assert(!std::three_way_comparable<OpaqueCost>);

class ScoreComponent
{
public:
    using value_type = ScoreValue;

    explicit ScoreComponent(const Instance&) noexcept
    {
    }

    [[nodiscard]]
    constexpr auto evaluate(const Solution& solution) const noexcept
        -> value_type
    {
        return value_type{.value = solution.score};
    }
};

class MaximizingSolutionManager
{
public:
    using input_type = Instance;
    using solution_type = Solution;

    explicit MaximizingSolutionManager(const Instance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto input() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]]
    constexpr auto is_valid(const Solution&) const noexcept -> bool
    {
        return true;
    }

private:
    const Instance& instance_;
};

struct MaximizingAggregator
{
    [[nodiscard]]
    constexpr auto operator()(const ScoreValue& value) const noexcept
        -> OpaqueCost
    {
        return OpaqueCost{.value = value.value};
    }

    // Larger values are better: compare orders them first.
    [[nodiscard]]
    static constexpr auto compare(const OpaqueCost& lhs, const OpaqueCost& rhs) noexcept
        -> std::partial_ordering
    {
        return rhs.value <=> lhs.value;
    }
};

class IntegerSolutionManager
{
public:
    using input_type = Instance;
    using solution_type = Solution;

    explicit IntegerSolutionManager(const Instance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto input() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]]
    constexpr auto is_valid(const Solution&) const noexcept -> bool
    {
        return true;
    }


private:
    const Instance& instance_;
};

struct IntegerAggregator
{
    [[nodiscard]]
    constexpr auto operator()(const ScoreValue& value) const noexcept -> int
    {
        return value.value;
    }
};

class OpaqueSolutionManager
{
public:
    using input_type = Instance;
    using solution_type = Solution;

    explicit OpaqueSolutionManager(const Instance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto input() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]]
    constexpr auto is_valid(const Solution&) const noexcept -> bool
    {
        return true;
    }

private:
    const Instance& instance_;
};

class NeighborhoodExplorer
{
public:
    using input_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    template<class SolutionManager>
    explicit NeighborhoodExplorer(const SolutionManager& solution_manager) noexcept
        : instance_{solution_manager.input()}
    {
    }

    [[nodiscard]]
    auto input() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]]
    static constexpr auto moves(const Solution&) noexcept
    {
        return std::array{
            Move{.delta = 1},
            Move{.delta = 2},
            Move{.delta = -1},
        };
    }


    [[nodiscard]] static constexpr auto is_valid(const Solution&, const Move&) noexcept -> bool { return true; }

    template<class RNG>
    [[nodiscard]]
    static auto random_move(const Solution&, RNG& rng) -> std::optional<Move>
    {
        return Move{.delta = std::uniform_int_distribution<int>{0, 1}(rng) == 0 ? 1 : -1};
    }

    static constexpr void make_move(
        Solution& solution,
        const Move& move) noexcept
    {
        solution.score += move.delta;
    }

private:
    const Instance& instance_;
};

struct SemanticProbeResult
{
    bool better;
    bool equivalent;
    bool better_or_equivalent_better;
    bool better_or_equivalent_equal;
    bool worse_or_equivalent;
};

struct MaximizingSemanticProbe
{
    template<class Context>
        requires requires(
            const Context& context,
            const typename Context::cost_type& lhs,
            const typename Context::cost_type& rhs)
        {
            { context.better(lhs, rhs) } -> std::convertible_to<bool>;
            { context.equivalent(lhs, rhs) } -> std::convertible_to<bool>;
            {
                context.better_or_equivalent(lhs, rhs)
            } -> std::convertible_to<bool>;
        }
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution) const -> SemanticProbeResult
    {
        const auto low = context.solution_manager().evaluate(solution);
        ++solution.score;
        const auto high = context.solution_manager().evaluate(solution);

        return SemanticProbeResult{
            .better = context.better(high, low),
            .equivalent = context.equivalent(low, low),
            .better_or_equivalent_better =
                context.better_or_equivalent(high, low),
            .better_or_equivalent_equal =
                context.better_or_equivalent(low, low),
            .worse_or_equivalent =
                context.better_or_equivalent(low, high),
        };
    }
};

struct MinimizingSemanticProbe
{
    template<class Context>
        requires requires(
            const Context& context,
            const typename Context::cost_type& lhs,
            const typename Context::cost_type& rhs)
        {
            { context.better(lhs, rhs) } -> std::convertible_to<bool>;
            { context.equivalent(lhs, rhs) } -> std::convertible_to<bool>;
            {
                context.better_or_equivalent(lhs, rhs)
            } -> std::convertible_to<bool>;
        }
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution) const -> SemanticProbeResult
    {
        const auto low = context.solution_manager().evaluate(solution);
        ++solution.score;
        const auto high = context.solution_manager().evaluate(solution);

        return SemanticProbeResult{
            .better = context.better(low, high),
            .equivalent = context.equivalent(low, low),
            .better_or_equivalent_better =
                context.better_or_equivalent(low, high),
            .better_or_equivalent_equal =
                context.better_or_equivalent(low, low),
            .worse_or_equivalent =
                context.better_or_equivalent(high, low),
        };
    }
};

auto expect(const bool condition, const std::string_view description) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }

    return true;
}

} // namespace

int main()
{
    using easylocal::Runner;
    using easylocal::component;
    using easylocal::neighborhood;
    using easylocal::solution_manager;

    bool ok = true;
    const Instance instance;

    const auto maximizing_manager =
        solution_manager<MaximizingSolutionManager>()
        | easylocal::cost::apply(
              MaximizingAggregator{},
              component<ScoreComponent>());

    auto maximizing_probe =
        Runner{MaximizingSemanticProbe{}}
        | maximizing_manager
        | neighborhood<NeighborhoodExplorer>();

    const auto maximizing =
        maximizing_probe.bind(instance).run(Solution{.score = 1});

    ok &= expect(maximizing.better,
        "custom cost semantics can define larger values as better");
    ok &= expect(maximizing.equivalent,
        "custom semantic equivalence is forwarded through a configured manager");
    ok &= expect(maximizing.better_or_equivalent_better,
        "custom better_or_equivalent accepts a strictly better value");
    ok &= expect(maximizing.better_or_equivalent_equal,
        "custom better_or_equivalent accepts a semantically equivalent value");
    ok &= expect(!maximizing.worse_or_equivalent,
        "custom better_or_equivalent rejects a worse value");

    auto first_improvement =
        easylocal::make_runner<FirstImprovement>(FirstImprovementParameters{
            .max_evaluations = 2,
        })
        | maximizing_manager | neighborhood<NeighborhoodExplorer>();

    const auto first_result =
        first_improvement.bind(instance).run(Solution{.score = 0});

    ok &= expect(first_result.solution.score == 1,
        "First Improvement follows semantic preference, not numeric <");
    ok &= expect(first_result.cost.value == 1,
        "First Improvement returns the maximizing opaque cost");

    auto best_improvement =
        easylocal::make_runner<BestImprovement>(BestImprovementParameters{
            .max_evaluations = 4,
        })
        | maximizing_manager | neighborhood<NeighborhoodExplorer>();

    const auto best_result =
        best_improvement.bind(instance).run(Solution{.score = 0});

    ok &= expect(best_result.solution.score == 2,
        "Best Improvement selects the semantically best maximizing candidate");
    ok &= expect(best_result.cost.value == 2,
        "Best Improvement does not require an intrinsic order on cost_type");

    // Hill Climbing accepts sideways moves with better_or_equivalent: derived
    // from the same compare as better, it climbs towards larger values.
    auto hill_climbing = easylocal::make_runner<easylocal::runners::HillClimbing>()
        | maximizing_manager | neighborhood<NeighborhoodExplorer>();
    std::mt19937 rng{7U};
    const auto climbed = hill_climbing.bind(instance).run(
        Solution{.score = 0},
        rng,
        easylocal::max_evaluations(200));
    ok &= expect(
        climbed.solution.score > 50 && climbed.cost.value == climbed.solution.score,
        "Hill Climbing follows a maximizing compare, accepting no worse move");

    const auto minimizing_manager =
        solution_manager<IntegerSolutionManager>()
        | easylocal::cost::apply(
              IntegerAggregator{},
              component<ScoreComponent>());

    auto minimizing_probe =
        Runner{MinimizingSemanticProbe{}}
        | minimizing_manager
        | neighborhood<NeighborhoodExplorer>();

    const auto minimizing =
        minimizing_probe.bind(instance).run(Solution{.score = 1});

    ok &= expect(minimizing.better,
        "ordinary < remains the default better relation");
    ok &= expect(minimizing.equivalent,
        "ordinary == remains the default equivalent relation");
    ok &= expect(minimizing.better_or_equivalent_better,
        "ordinary <= remains the default better_or_equivalent relation");
    ok &= expect(minimizing.better_or_equivalent_equal,
        "default better_or_equivalent includes equality");
    ok &= expect(!minimizing.worse_or_equivalent,
        "default better_or_equivalent rejects a numerically worse value");


    return ok ? 0 : 1;
}
