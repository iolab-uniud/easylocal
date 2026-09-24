#include "best_improvement.hpp"
#include "first_improvement.hpp"
#include "move.hpp"
#include "neighborhood_explorer.hpp"
#include "random_first_improvement.hpp"
#include "sampling.hpp"
#include "solution_manager.hpp"

#include <easylocal/easylocal.hpp>

#include <concepts>
#include <cstddef>
#include <iostream>
#include <random>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace
{

using namespace easylocal::mwe::assignment;

struct SwapMove
{
    std::size_t first;
    std::size_t second;
};

class SwapNeighborhoodExplorer
{
public:
    using instance_type = Instance;
    using solution_type = Solution;
    using move_type = SwapMove;
    using random_sampling = sampling::without_replacement;

    explicit SwapNeighborhoodExplorer(
        const SolutionManager& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const Instance&
    {
        return solution_manager_.instance();
    }

    [[nodiscard]]
    auto moves(const Solution& solution) const
    {
        const auto size = solution.assignment.size();

        return std::views::iota(std::size_t{0}, size * size)
             | std::views::filter([size](const std::size_t ordinal) {
                   const auto first = ordinal / size;
                   const auto second = ordinal % size;
                   return first < second;
               })
             | std::views::transform([size](const std::size_t ordinal) {
                   return SwapMove{
                       .first = ordinal / size,
                       .second = ordinal % size,
                   };
               });
    }

    void make_move(Solution& solution, const SwapMove move) const
    {
        std::swap(
            solution.assignment[move.first],
            solution.assignment[move.second]);
    }

private:
    const SolutionManager& solution_manager_;
};

class DestinationZeroNeighborhoodExplorer
{
public:
    using instance_type = Instance;
    using solution_type = Solution;
    using move_type = Move;
    using random_sampling = sampling::without_replacement;

    explicit DestinationZeroNeighborhoodExplorer(
        const SolutionManager& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const Instance&
    {
        return solution_manager_.instance();
    }

    [[nodiscard]]
    auto moves(const Solution& solution) const
    {
        return std::views::iota(std::size_t{0}, solution.assignment.size())
             | std::views::filter([&solution](const std::size_t job) {
                   return solution.assignment[job] != 0;
               })
             | std::views::transform([](const std::size_t job) {
                   return Move{
                       .job = job,
                       .destination = 0,
                   };
               });
    }

    void make_move(Solution& solution, const Move move) const
    {
        solution.assignment[move.job] = move.destination;
    }

private:
    const SolutionManager& solution_manager_;
};

class CollectNeighborhoodEffects
{
public:
    template<class Context>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution) const
        -> std::vector<std::vector<machine_id>>
    {
        const auto& neighborhood = context.neighborhood_explorer();
        std::vector<std::vector<machine_id>> effects;

        for (const auto move : neighborhood.moves(solution))
        {
            auto candidate = solution;
            neighborhood.make_move(candidate, move);
            effects.push_back(std::move(candidate.assignment));
        }

        return effects;
    }
};

template<class BoundRunner, class RNG>
concept CanRunWithRng =
    requires(BoundRunner& runner, Solution solution, RNG& rng) {
        runner.run(std::move(solution), rng);
    };

template<class NHE, class RNG>
concept HasRandomMoves =
    requires(const NHE& neighborhood, const Solution& solution, RNG& rng) {
        neighborhood.random_moves(solution, rng);
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
    using easylocal::neighborhood;
    using easylocal::neighborhood_union;
    using easylocal::solution_manager;

    bool ok = true;

    const Instance instance{
        .demand = {4, 3, 2},
        .capacity = {5, 5},
    };
    const Solution initial{
        .assignment = {0, 0, 1},
    };

    auto union_spec = neighborhood_union(
        neighborhood<NeighborhoodExplorer>(),
        neighborhood<SwapNeighborhoodExplorer>(),
        neighborhood<DestinationZeroNeighborhoodExplorer>());

    using UnionExplorer = typename decltype(union_spec)::service_type;

    static_assert(std::variant_size_v<typename UnionExplorer::move_type> == 3);
    static_assert(!HasRandomMoves<UnionExplorer, std::mt19937>);

    using FluentRunner = decltype(
        Runner{FirstImprovement{{.max_evaluations = 32}}}
            .with_solution_manager<SolutionManager>()
            .with_neighborhood(neighborhood_union(
                neighborhood<NeighborhoodExplorer>(),
                neighborhood<SwapNeighborhoodExplorer>(),
                neighborhood<DestinationZeroNeighborhoodExplorer>())));

    using PipelineRunner = decltype(
        Runner{FirstImprovement{{.max_evaluations = 32}}}
        | solution_manager<SolutionManager>()
        | neighborhood_union(
              neighborhood<NeighborhoodExplorer>(),
              neighborhood<SwapNeighborhoodExplorer>(),
              neighborhood<DestinationZeroNeighborhoodExplorer>()));

    static_assert(std::same_as<FluentRunner, PipelineRunner>);

    auto collector =
        Runner{CollectNeighborhoodEffects{}}
        | solution_manager<SolutionManager>()
        | union_spec;

    auto bound_collector = collector.bind(instance);
    const auto effects = bound_collector.run(initial);

    const std::vector<std::vector<machine_id>> expected_effects{
        {1, 0, 1},
        {0, 1, 1},
        {0, 0, 0},
        {0, 0, 1},
        {1, 0, 0},
        {0, 1, 0},
        {0, 0, 0},
    };

    ok &= expect(
        effects == expected_effects,
        "n-ary union lazily concatenates child neighborhoods in declaration order and dispatches moves to the originating child");

    auto first_runner =
        Runner{FirstImprovement{{.max_evaluations = 32}}}
        | solution_manager<SolutionManager>()
        | neighborhood_union(
              neighborhood<NeighborhoodExplorer>(),
              neighborhood<SwapNeighborhoodExplorer>(),
              neighborhood<DestinationZeroNeighborhoodExplorer>());

    auto bound_first = first_runner.bind(instance);
    const auto first_result = bound_first.run(initial);

    ok &= expect(
        first_result.cost == Cost{0, 0},
        "first improvement consumes a neighborhood union without algorithm changes");

    auto best_runner =
        Runner{BestImprovement{{.max_evaluations = 64}}}
        | solution_manager<SolutionManager>()
        | neighborhood_union(
              neighborhood<NeighborhoodExplorer>(),
              neighborhood<SwapNeighborhoodExplorer>(),
              neighborhood<DestinationZeroNeighborhoodExplorer>());

    auto bound_best = best_runner.bind(instance);
    const auto best_result = bound_best.run(initial);

    ok &= expect(
        best_result.cost == Cost{0, 0},
        "best improvement consumes a neighborhood union without algorithm changes");

    auto random_runner =
        Runner{RandomFirstImprovement{{.max_evaluations = 32}}}
        | solution_manager<SolutionManager>()
        | neighborhood_union(
              neighborhood<NeighborhoodExplorer>(),
              neighborhood<SwapNeighborhoodExplorer>(),
              neighborhood<DestinationZeroNeighborhoodExplorer>());

    using BoundRandomRunner = decltype(random_runner.bind(instance));
    static_assert(!CanRunWithRng<BoundRandomRunner, std::mt19937>);

    return ok ? 0 : 1;
}
