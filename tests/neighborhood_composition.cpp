#include <easylocal/easylocal.hpp>
#include <easylocal/search/best_improvement.hpp>
#include <easylocal/search/first_improvement.hpp>

#include "move.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <concepts>
#include <cstddef>
#include <iostream>
#include <limits>
#include <optional>
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
using easylocal::search::BestImprovement;
using easylocal::search::FirstImprovement;

[[nodiscard]]
auto default_solution_manager_recipe()
{
    return easylocal::solution_manager<AssignmentSolutionManager>()
         | easylocal::component<CapacityCostComponent>();
}

struct SwapMove
{
    std::size_t first;
    std::size_t second;
};

class SwapNeighborhoodExplorer
{
public:
    using instance_type = AssignmentInstance;
    using solution_type = AssignmentSolution;
    using move_type = SwapMove;

    explicit SwapNeighborhoodExplorer(
        const AssignmentSolutionManager& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const AssignmentInstance&
    {
        return solution_manager_.instance();
    }

    [[nodiscard]]
    auto moves(const AssignmentSolution& solution) const
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

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto random_move(
        const AssignmentSolution& solution,
        RNG& rng) const -> std::optional<SwapMove>
    {
        const auto size = solution.assignment.size();
        if (size < 2)
        {
            return std::nullopt;
        }

        const auto count = size * (size - 1) / 2;
        if (count == 0)
        {
            return std::nullopt;
        }

        std::uniform_int_distribution<std::size_t> draw{0, count - 1};
        auto ordinal = draw(rng);

        for (std::size_t first = 0; first < size; ++first)
        {
            const auto row = size - first - 1;
            if (ordinal < row)
            {
                return SwapMove{
                    .first = first,
                    .second = first + 1 + ordinal,
                };
            }
            ordinal -= row;
        }

        return std::nullopt;
    }

    void make_move(AssignmentSolution& solution, const SwapMove move) const
    {
        std::swap(
            solution.assignment[move.first],
            solution.assignment[move.second]);
    }

private:
    const AssignmentSolutionManager& solution_manager_;
};

class DestinationZeroNeighborhoodExplorer
{
public:
    using instance_type = AssignmentInstance;
    using solution_type = AssignmentSolution;
    using move_type = ReassignJobMove;

    explicit DestinationZeroNeighborhoodExplorer(
        const AssignmentSolutionManager& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const AssignmentInstance&
    {
        return solution_manager_.instance();
    }

    [[nodiscard]]
    auto moves(const AssignmentSolution& solution) const
    {
        return std::views::iota(std::size_t{0}, solution.assignment.size())
             | std::views::filter([&solution](const std::size_t job) {
                   return solution.assignment[job] != 0;
               })
             | std::views::transform([](const std::size_t job) {
                   return ReassignJobMove{
                       .job = job,
                       .destination = 0,
                   };
               });
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto random_move(
        const AssignmentSolution& solution,
        RNG& rng) const -> std::optional<ReassignJobMove>
    {
        const auto count = static_cast<std::size_t>(std::ranges::count_if(
            solution.assignment,
            [](const machine_id machine) { return machine != 0; }));

        if (count == 0)
        {
            return std::nullopt;
        }

        std::uniform_int_distribution<std::size_t> draw{0, count - 1};
        auto target = draw(rng);

        for (std::size_t job = 0; job < solution.assignment.size(); ++job)
        {
            if (solution.assignment[job] == 0)
            {
                continue;
            }

            if (target == 0)
            {
                return ReassignJobMove{.job = job, .destination = 0};
            }
            --target;
        }

        return std::nullopt;
    }

    void make_move(AssignmentSolution& solution, const ReassignJobMove move) const
    {
        solution.assignment[move.job] = move.destination;
    }

private:
    const AssignmentSolutionManager& solution_manager_;
};

class DeterministicOnlyNeighborhoodExplorer
{
public:
    using instance_type = AssignmentInstance;
    using solution_type = AssignmentSolution;
    using move_type = ReassignJobMove;

    explicit DeterministicOnlyNeighborhoodExplorer(
        const AssignmentSolutionManager& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const AssignmentInstance&
    {
        return solution_manager_.instance();
    }

    [[nodiscard]]
    auto moves(const AssignmentSolution&) const
    {
        return std::views::empty<ReassignJobMove>;
    }

    void make_move(AssignmentSolution&, const ReassignJobMove) const noexcept
    {
    }

private:
    const AssignmentSolutionManager& solution_manager_;
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

template<class NHE, class RNG>
concept HasRandomMove =
    requires(const NHE& neighborhood, const AssignmentSolution& solution, RNG& rng) {
        neighborhood.random_move(solution, rng);
    };

class SampleNeighborhoodIndex
{
public:
    template<class Context, std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto run(
        const Context& context,
        const typename Context::solution_type& solution,
        RNG& rng) const -> std::size_t
    {
        const auto move =
            context.neighborhood_explorer().random_move(solution, rng);
        return move
            ? move->index()
            : std::numeric_limits<std::size_t>::max();
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
    using easylocal::neighborhood;
    using easylocal::neighborhood_union;
    using easylocal::random_biases;
    using easylocal::solution_manager;

    bool ok = true;

    const AssignmentInstance instance{
        .demand = {4, 3, 2},
        .capacity = {5, 5},
    };
    const AssignmentSolution initial{
        .assignment = {0, 0, 1},
    };

    auto union_spec = neighborhood_union(
        neighborhood<ReassignJobNeighborhoodExplorer>(),
        neighborhood<SwapNeighborhoodExplorer>(),
        neighborhood<DestinationZeroNeighborhoodExplorer>());

    using UnionExplorer = typename decltype(union_spec)::service_type;

    static_assert(std::variant_size_v<typename UnionExplorer::move_type> == 3);
    static_assert(HasRandomMove<UnionExplorer, std::mt19937>);

    using PartiallyRandomUnionExplorer = typename decltype(neighborhood_union(
        neighborhood<ReassignJobNeighborhoodExplorer>(),
        neighborhood<DeterministicOnlyNeighborhoodExplorer>()))::service_type;
    static_assert(!HasRandomMove<PartiallyRandomUnionExplorer, std::mt19937>);

    using FluentRunner = decltype(
        Runner{FirstImprovement{{.max_evaluations = 32}}}
            .with_solution_manager(default_solution_manager_recipe())
            .with_neighborhood(neighborhood_union(
                neighborhood<ReassignJobNeighborhoodExplorer>(),
                neighborhood<SwapNeighborhoodExplorer>(),
                neighborhood<DestinationZeroNeighborhoodExplorer>())));

    using PipelineRunner = decltype(
        Runner{FirstImprovement{{.max_evaluations = 32}}}
        | default_solution_manager_recipe()
        | neighborhood_union(
              neighborhood<ReassignJobNeighborhoodExplorer>(),
              neighborhood<SwapNeighborhoodExplorer>(),
              neighborhood<DestinationZeroNeighborhoodExplorer>()));

    static_assert(std::same_as<FluentRunner, PipelineRunner>);

    auto collector =
        Runner{CollectNeighborhoodEffects{}}
        | default_solution_manager_recipe()
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
        | default_solution_manager_recipe()
        | neighborhood_union(
              neighborhood<ReassignJobNeighborhoodExplorer>(),
              neighborhood<SwapNeighborhoodExplorer>(),
              neighborhood<DestinationZeroNeighborhoodExplorer>());

    auto bound_first = first_runner.bind(instance);
    const auto first_result = bound_first.run(initial);

    ok &= expect(
        first_result.cost == Cost{0, 0},
        "first improvement consumes a neighborhood union without algorithm changes");

    auto best_runner =
        Runner{BestImprovement{{.max_evaluations = 64}}}
        | default_solution_manager_recipe()
        | neighborhood_union(
              neighborhood<ReassignJobNeighborhoodExplorer>(),
              neighborhood<SwapNeighborhoodExplorer>(),
              neighborhood<DestinationZeroNeighborhoodExplorer>());

    auto bound_best = best_runner.bind(instance);
    const auto best_result = bound_best.run(initial);

    ok &= expect(
        best_result.cost == Cost{0, 0},
        "best improvement consumes a neighborhood union without algorithm changes");

    auto biased_sampler =
        Runner{SampleNeighborhoodIndex{}}
        | default_solution_manager_recipe()
        | (neighborhood_union(
               neighborhood<ReassignJobNeighborhoodExplorer>(),
               neighborhood<SwapNeighborhoodExplorer>(),
               neighborhood<DestinationZeroNeighborhoodExplorer>())
           | random_biases(0.0, 1.0, 0.0));

    auto bound_biased_sampler = biased_sampler.bind(instance);
    std::mt19937 biased_rng{12345};
    for (std::size_t sample = 0; sample < 32; ++sample)
    {
        ok &= expect(
            bound_biased_sampler.run(initial, biased_rng) == 1,
            "zero random bias disables a child while a positive bias selects the enabled neighborhood");
    }

    auto disabled_sampler =
        Runner{SampleNeighborhoodIndex{}}
        | default_solution_manager_recipe()
        | (neighborhood_union(
               neighborhood<ReassignJobNeighborhoodExplorer>(),
               neighborhood<SwapNeighborhoodExplorer>(),
               neighborhood<DestinationZeroNeighborhoodExplorer>())
           | random_biases(0.0, 0.0, 0.0));

    auto bound_disabled_sampler = disabled_sampler.bind(instance);
    std::mt19937 disabled_rng{1};
    ok &= expect(
        bound_disabled_sampler.run(initial, disabled_rng) ==
            std::numeric_limits<std::size_t>::max(),
        "all-zero random biases disable random proposals without changing deterministic traversal");

    auto default_sampler =
        Runner{SampleNeighborhoodIndex{}}
        | default_solution_manager_recipe()
        | neighborhood_union(
              neighborhood<ReassignJobNeighborhoodExplorer>(),
              neighborhood<SwapNeighborhoodExplorer>(),
              neighborhood<DestinationZeroNeighborhoodExplorer>());

    auto bound_default_sampler = default_sampler.bind(instance);
    std::mt19937 default_rng_a{67890};
    std::mt19937 default_rng_b{67890};
    for (std::size_t sample = 0; sample < 32; ++sample)
    {
        ok &= expect(
            bound_default_sampler.run(initial, default_rng_a) ==
                bound_default_sampler.run(initial, default_rng_b),
            "neighborhood-union random selection is reproducible for identical RNG state");
    }

    return ok ? 0 : 1;
}
