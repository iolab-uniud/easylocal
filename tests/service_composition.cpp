#include "capacity_delta.hpp"
#include <easylocal/search/first_improvement.hpp>
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/aggregation.hpp>
#include <easylocal/detail/evaluation.hpp>
#include <easylocal/detail/service_composition.hpp>
#include <easylocal/runner.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <ranges>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace
{

using namespace easylocal::mwe::assignment;
using easylocal::search::FirstImprovement;

class AssignmentCardinalityComponent
{
public:
    using value_type = std::size_t;

    explicit AssignmentCardinalityComponent(const Instance&) noexcept
    {
    }

    [[nodiscard]]
    auto evaluate(const Solution& solution) const noexcept -> value_type
    {
        return solution.assignment.size();
    }
};

class AssignmentCardinalityDeltaEvaluator
{
public:
    explicit AssignmentCardinalityDeltaEvaluator(const Instance&) noexcept
    {
    }

    [[nodiscard]]
    auto delta_evaluate(const Solution&, const Move&) const noexcept
        -> std::size_t
    {
        return 0;
    }
};

class IncompatibleCapacityDeltaEvaluator
{
public:
    explicit IncompatibleCapacityDeltaEvaluator(const Instance&) noexcept
    {
    }

    [[nodiscard]]
    auto delta_evaluate(const Solution&, const Move&) const noexcept
        -> const char*
    {
        return "not a capacity delta";
    }
};

class TwoStageSolutionManager : public SolutionManager
{
public:
    using SolutionManager::SolutionManager;
    using SolutionManager::aggregate;

    [[nodiscard]]
    auto aggregate(
        const CapacityValue& capacity,
        const std::size_t cardinality) const
    {
        return easylocal::aggregation::hierarchical{}(
            capacity.total_overload,
            capacity.overloaded_machines,
            cardinality);
    }
};

class CountingSingleMoveNeighborhood
{
public:
    using instance_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    CountingSingleMoveNeighborhood(
        const SolutionManager& solution_manager,
        const Move move,
        int& make_move_count) noexcept
        : solution_manager_{solution_manager},
          move_{move},
          make_move_count_{make_move_count}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const Instance&
    {
        return solution_manager_.instance();
    }

    [[nodiscard]]
    auto moves(const Solution&) const
    {
        return std::views::single(move_);
    }

    void make_move(Solution& solution, const Move& move) const noexcept
    {
        ++make_move_count_.get();
        solution.assignment[move.job] = move.destination;
    }

private:
    const SolutionManager& solution_manager_;
    Move move_;
    std::reference_wrapper<int> make_move_count_;
};

class CapacityVariantA : public CapacityCostComponent
{
public:
    using CapacityCostComponent::CapacityCostComponent;
};

class CapacityVariantB : public CapacityCostComponent
{
public:
    using CapacityCostComponent::CapacityCostComponent;
};

class TwoCapacitySolutionManager : public SolutionManager
{
public:
    using SolutionManager::SolutionManager;

    [[nodiscard]]
    auto aggregate(
        const CapacityValue& first,
        const CapacityValue& second) const
    {
        return easylocal::aggregation::lexicographic{}(
            first.total_overload,
            second.total_overload);
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
    using easylocal::delta;
    using easylocal::neighborhood;
    using easylocal::solution_manager;

    int type_only_make_move_count = 0;

    using BareSMRecipe = decltype(solution_manager<SolutionManager>());
    using BareNHERecipe = decltype(neighborhood<NeighborhoodExplorer>());
    static_assert(std::same_as<
        typename BareSMRecipe::service_type,
        SolutionManager>);
    static_assert(std::same_as<
        typename BareNHERecipe::service_type,
        NeighborhoodExplorer>);

    using CapacitySpec = decltype(component<CapacityCostComponent>());
    using CardinalitySpec = decltype(component<AssignmentCardinalityComponent>());
    using CapacityDeltaSpec = decltype(delta<
        CapacityCostComponent,
        ReassignCapacityDeltaEvaluator>());
    using SecondCapacityDeltaSpec = decltype(delta<
        CapacityCostComponent,
        IncompatibleCapacityDeltaEvaluator>());

    using FluentSMRecipe = decltype(
        solution_manager<TwoStageSolutionManager>()
            .with_component<CapacityCostComponent>());
    using PipedSMRecipe = decltype(
        solution_manager<TwoStageSolutionManager>()
        | component<CapacityCostComponent>());
    static_assert(std::same_as<FluentSMRecipe, PipedSMRecipe>);

    using FluentNHERecipe = decltype(
        neighborhood<CountingSingleMoveNeighborhood>(
            Move{.job = 1, .destination = 1},
            std::ref(type_only_make_move_count))
            .with_delta<
                CapacityCostComponent,
                ReassignCapacityDeltaEvaluator>());
    using PipedNHERecipe = decltype(
        neighborhood<CountingSingleMoveNeighborhood>(
            Move{.job = 1, .destination = 1},
            std::ref(type_only_make_move_count))
        | delta<
              CapacityCostComponent,
              ReassignCapacityDeltaEvaluator>());
    static_assert(std::same_as<FluentNHERecipe, PipedNHERecipe>);

    static_assert(easylocal::detail::unique_component_specs_v<
        CapacitySpec,
        CardinalitySpec>);
    static_assert(!easylocal::detail::unique_component_specs_v<
        CapacitySpec,
        CapacitySpec>);
    static_assert(!easylocal::detail::unique_delta_component_specs_v<
        CapacityDeltaSpec,
        SecondCapacityDeltaSpec>);

    // Distinct subclasses are distinct component identities, even when they
    // reuse the same implementation and value_type.
    using VariantASpec = decltype(component<CapacityVariantA>());
    using VariantBSpec = decltype(component<CapacityVariantB>());
    static_assert(easylocal::detail::unique_component_specs_v<
        VariantASpec,
        VariantBSpec>);
    static_assert(!std::same_as<CapacityVariantA, CapacityVariantB>);

    const auto two_capacity_recipe =
        solution_manager<TwoCapacitySolutionManager>()
        | component<CapacityVariantA>()
        | component<CapacityVariantB>();
    using TwoCapacityConfigured =
        typename decltype(two_capacity_recipe)::service_type;
    static_assert(std::tuple_size_v<
        typename TwoCapacityConfigured::component_types> == 2);

    const auto hard_manager_recipe =
        solution_manager<TwoStageSolutionManager>()
        | component<CapacityCostComponent>();
    const auto full_manager_recipe =
        solution_manager<TwoStageSolutionManager>()
        | component<CapacityCostComponent>()
        | component<AssignmentCardinalityComponent>();

    const auto hard_neighborhood_recipe =
        neighborhood<CountingSingleMoveNeighborhood>(
            Move{.job = 1, .destination = 1},
            std::ref(type_only_make_move_count))
        | delta<CapacityCostComponent, ReassignCapacityDeltaEvaluator>();

    // The previous recipe is used only for type-level checks; no construction
    // occurs, so the placeholder reference is never observed.
    using HardSM = typename decltype(hard_manager_recipe)::service_type;
    using HardNHE = typename decltype(hard_neighborhood_recipe)::service_type;
    static_assert(easylocal::detail::all_delta_components_active_v<
        HardSM,
        HardNHE>);
    static_assert(easylocal::detail::all_delta_bindings_compatible_v<
        HardSM,
        HardNHE>);

    const auto inactive_delta_recipe =
        neighborhood<CountingSingleMoveNeighborhood>(
            Move{.job = 1, .destination = 1},
            std::ref(type_only_make_move_count))
        | delta<
              AssignmentCardinalityComponent,
              AssignmentCardinalityDeltaEvaluator>();
    using InactiveNHE =
        typename decltype(inactive_delta_recipe)::service_type;
    static_assert(!easylocal::detail::all_delta_components_active_v<
        HardSM,
        InactiveNHE>);

    const auto incompatible_delta_recipe =
        neighborhood<CountingSingleMoveNeighborhood>(
            Move{.job = 1, .destination = 1},
            std::ref(type_only_make_move_count))
        | delta<
              CapacityCostComponent,
              IncompatibleCapacityDeltaEvaluator>();
    using IncompatibleNHE =
        typename decltype(incompatible_delta_recipe)::service_type;
    static_assert(easylocal::detail::all_delta_components_active_v<
        HardSM,
        IncompatibleNHE>);
    static_assert(!easylocal::detail::all_delta_bindings_compatible_v<
        HardSM,
        IncompatibleNHE>);

    bool ok = true;

    const Instance instance{
        .demand = {4, 3, 2},
        .capacity = {5, 5},
    };
    const Solution initial{
        .assignment = {0, 0, 1},
    };
    const Move relieving_move{
        .job = 1,
        .destination = 1,
    };

    const auto two_capacity_manager = two_capacity_recipe.construct(instance);
    const auto two_capacity_cost = two_capacity_manager.evaluate(initial);

    ok &= expect(
        two_capacity_cost.get<0>() == 2 &&
            two_capacity_cost.get<1>() == 2,
        "distinct component subclasses are independently composed and evaluated");

    int variant_make_moves = 0;
    auto variant_runner =
        Runner{FirstImprovement{{.max_evaluations = 2}}}
        | two_capacity_recipe
        | (neighborhood<CountingSingleMoveNeighborhood>(
               relieving_move,
               std::ref(variant_make_moves))
           | delta<CapacityVariantA, ReassignCapacityDeltaEvaluator>()
           | delta<CapacityVariantB, ReassignCapacityDeltaEvaluator>());

    const auto variant_result = variant_runner.bind(instance).run(initial);

    ok &= expect(
        variant_result.cost.get<0>() == 0 &&
            variant_result.cost.get<1>() == 0,
        "the same delta evaluator type can be attached under distinct component identities");
    ok &= expect(
        variant_make_moves == 1,
        "distinct component identities remain an all-delta configuration");

    int hard_make_moves = 0;
    const auto hard_nhe =
        neighborhood<CountingSingleMoveNeighborhood>(
            relieving_move,
            std::ref(hard_make_moves))
        | delta<CapacityCostComponent, ReassignCapacityDeltaEvaluator>();

    auto hard_runner =
        Runner{FirstImprovement{{.max_evaluations = 2}}}
        | hard_manager_recipe
        | hard_nhe;

    const auto hard_result = hard_runner.bind(instance).run(initial);

    ok &= expect(
        hard_result.cost == Cost{0, 0},
        "hard-only stage uses its local capacity delta configuration");
    ok &= expect(
        hard_make_moves == 1,
        "hard-only all-delta stage applies the accepted move exactly once");

    int full_make_moves = 0;
    const auto full_nhe =
        neighborhood<CountingSingleMoveNeighborhood>(
            relieving_move,
            std::ref(full_make_moves))
        | delta<CapacityCostComponent, ReassignCapacityDeltaEvaluator>()
        | delta<
              AssignmentCardinalityComponent,
              AssignmentCardinalityDeltaEvaluator>();

    auto full_runner =
        Runner{FirstImprovement{{.max_evaluations = 2}}}
        | full_manager_recipe
        | full_nhe;

    const auto full_result = full_runner.bind(instance).run(initial);

    ok &= expect(
        full_result.cost.get<0>() == 0 &&
            full_result.cost.get<1>() == 0 &&
            full_result.cost.get<2>() == 3,
        "hard+soft stage uses a distinct local delta set on the same Solution and Move types");
    ok &= expect(
        full_make_moves == 1,
        "hard+soft all-delta stage applies the accepted move exactly once");

    // A full-stage delta set is invalid when paired with a hard-only manager:
    // bind() uses the same predicate in a static_assert.
    using FullNHE = typename decltype(full_nhe)::service_type;
    static_assert(!easylocal::detail::all_delta_components_active_v<
        HardSM,
        FullNHE>);

    return ok ? 0 : 1;
}
