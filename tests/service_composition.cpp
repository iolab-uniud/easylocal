#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"
#include "support/assignment_capacity_delta.hpp"

#include <easylocal/config/overrides.hpp>
#include <easylocal/config/tree.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/helpers/detail/evaluation.hpp>
#include <easylocal/helpers/detail/neighborhood_recipe.hpp>
#include <easylocal/helpers/detail/solution_manager_recipe.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/utils/logging.hpp>

#include <array>
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

using namespace assignment;
using easylocal::runners::FirstImprovement;

class AssignmentCardinalityComponent
{
public:
    using value_type = std::size_t;

    explicit AssignmentCardinalityComponent(const AssignmentInstance&) noexcept
    {
    }

    [[nodiscard]]
    auto evaluate(const AssignmentSolution& solution) const noexcept -> value_type
    {
        return solution.assignment.size();
    }
};

class ColocatedCardinalityComponent
{
public:
    explicit ColocatedCardinalityComponent(const AssignmentInstance&) noexcept
    {
    }

    [[nodiscard]]
    auto evaluate(const AssignmentSolution& solution) const noexcept -> std::size_t
    {
        return solution.assignment.size();
    }

    [[nodiscard]]
    auto delta_evaluate(
        const AssignmentSolution&,
        const ReassignJobMove&) const noexcept -> std::size_t
    {
        return 0;
    }
};

class StatelessOffsetComponent
{
public:
    explicit StatelessOffsetComponent(const std::size_t offset) noexcept
        : offset_{offset}
    {
    }

    [[nodiscard]]
    auto evaluate(const AssignmentSolution& solution) const noexcept -> std::size_t
    {
        return solution.assignment.size() + offset_;
    }

private:
    std::size_t offset_{};
};

class StatelessCardinalityDeltaEvaluator
{
public:
    [[nodiscard]]
    auto delta_evaluate(
        const AssignmentSolution&,
        const ReassignJobMove&) const noexcept -> std::size_t
    {
        return 0;
    }
};

class CountingSoftComponent
{
public:
    using value_type = std::size_t;

    CountingSoftComponent(
        const AssignmentInstance&,
        const std::reference_wrapper<int> evaluation_count) noexcept
        : evaluation_count_{evaluation_count}
    {
    }

    [[nodiscard]]
    auto evaluate(const AssignmentSolution& solution) const noexcept -> value_type
    {
        ++evaluation_count_.get();
        return solution.assignment.size();
    }

private:
    std::reference_wrapper<int> evaluation_count_;
};


class NoAggregateSolutionManager
    : public easylocal::solution_manager_base<
          AssignmentInstance,
          AssignmentSolution>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]] auto is_valid(const AssignmentSolution& solution) const noexcept -> bool
    {
        return solution.assignment.size() == input_.demand.size();
    }
};

class AssignmentCardinalityDeltaEvaluator
{
public:
    explicit AssignmentCardinalityDeltaEvaluator(const AssignmentInstance&) noexcept
    {
    }

    [[nodiscard]]
    auto delta_evaluate(const AssignmentSolution&, const ReassignJobMove&) const noexcept
        -> std::size_t
    {
        return 0;
    }
};

class IncompatibleCapacityDeltaEvaluator
{
public:
    explicit IncompatibleCapacityDeltaEvaluator(const AssignmentInstance&) noexcept
    {
    }

    [[nodiscard]]
    auto delta_evaluate(const AssignmentSolution&, const ReassignJobMove&) const noexcept
        -> const char*
    {
        return "not a capacity delta";
    }
};

class TwoStageSolutionManager : public AssignmentSolutionManager
{
public:
    using AssignmentSolutionManager::AssignmentSolutionManager;
};

class CountingSingleMoveNeighborhood
{
public:
    using input_type = AssignmentInstance;
    using solution_type = AssignmentSolution;
    using move_type = ReassignJobMove;

    CountingSingleMoveNeighborhood(
        const AssignmentSolutionManager& solution_manager,
        const ReassignJobMove move,
        int& make_move_count) noexcept
        : solution_manager_{solution_manager},
          move_{move},
          make_move_count_{make_move_count}
    {
    }

    [[nodiscard]]
    auto input() const noexcept -> const AssignmentInstance&
    {
        return solution_manager_.input();
    }

    [[nodiscard]]
    auto moves(const AssignmentSolution&) const
    {
        return std::views::single(move_);
    }

    [[nodiscard]] static auto is_valid(const AssignmentSolution&, const ReassignJobMove&) noexcept -> bool { return true; }

    void make_move(AssignmentSolution& solution, const ReassignJobMove& move) const noexcept
    {
        ++make_move_count_.get();
        solution.assignment[move.job] = move.destination;
    }

private:
    const AssignmentSolutionManager& solution_manager_;
    ReassignJobMove move_;
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

class TwoCapacitySolutionManager : public AssignmentSolutionManager
{
public:
    using AssignmentSolutionManager::AssignmentSolutionManager;
};

struct TwoCapacityAggregator
{
    [[nodiscard]]
    auto operator()(
        const CapacityValue& first,
        const CapacityValue& second) const
    {
        return easylocal::cost::lexicographic{
            first.total_overload,
            second.total_overload};
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

    using BareSMRecipe = decltype(solution_manager<AssignmentSolutionManager>());
    using BareNHERecipe = decltype(neighborhood<ReassignJobNeighborhoodExplorer>());
    static_assert(std::same_as<
        typename BareSMRecipe::service_type,
        AssignmentSolutionManager>);
    static_assert(std::same_as<
        typename BareNHERecipe::service_type,
        ReassignJobNeighborhoodExplorer>);

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
            .with_cost(component<CapacityCostComponent>()));
    using PipedSMRecipe = decltype(
        solution_manager<TwoStageSolutionManager>()
        | component<CapacityCostComponent>());
    static_assert(std::same_as<FluentSMRecipe, PipedSMRecipe>);

    using FluentNHERecipe = decltype(
        neighborhood<CountingSingleMoveNeighborhood>(
            ReassignJobMove{.job = 1, .destination = 1},
            std::ref(type_only_make_move_count))
            .with_delta<
                CapacityCostComponent,
                ReassignCapacityDeltaEvaluator>());
    using PipedNHERecipe = decltype(
        neighborhood<CountingSingleMoveNeighborhood>(
            ReassignJobMove{.job = 1, .destination = 1},
            std::ref(type_only_make_move_count))
        | delta<
              CapacityCostComponent,
              ReassignCapacityDeltaEvaluator>());
    static_assert(std::same_as<FluentNHERecipe, PipedNHERecipe>);

    using ColocatedFluentNHERecipe = decltype(
        neighborhood<CountingSingleMoveNeighborhood>(
            ReassignJobMove{.job = 1, .destination = 1},
            std::ref(type_only_make_move_count))
            .with_delta<ColocatedCardinalityComponent>());
    using ColocatedPipedNHERecipe = decltype(
        neighborhood<CountingSingleMoveNeighborhood>(
            ReassignJobMove{.job = 1, .destination = 1},
            std::ref(type_only_make_move_count))
        | delta<ColocatedCardinalityComponent>());
    static_assert(std::same_as<
        ColocatedFluentNHERecipe,
        ColocatedPipedNHERecipe>);

    static_assert(easylocal::detail::unique_types_v<
        typename CapacitySpec::component_type,
        typename CardinalitySpec::component_type>);
    static_assert(!easylocal::detail::unique_types_v<
        typename CapacitySpec::component_type,
        typename CapacitySpec::component_type>);
    static_assert(!easylocal::detail::unique_delta_component_specs_v<
        CapacityDeltaSpec,
        SecondCapacityDeltaSpec>);

    // Distinct subclasses are distinct component identities, even when they
    // reuse the same implementation and value_type.
    using VariantASpec = decltype(component<CapacityVariantA>());
    using VariantBSpec = decltype(component<CapacityVariantB>());
    static_assert(easylocal::detail::unique_types_v<
        typename VariantASpec::component_type,
        typename VariantBSpec::component_type>);
    static_assert(!std::same_as<CapacityVariantA, CapacityVariantB>);

    using FluentAggregatedRecipe = decltype(
        solution_manager<NoAggregateSolutionManager>()
            .with_cost(easylocal::cost::sum(easylocal::cost::weighted(
                component<AssignmentCardinalityComponent>(), 3))));
    using PipedAggregatedRecipe = decltype(
        solution_manager<NoAggregateSolutionManager>()
        | easylocal::cost::sum(easylocal::cost::weighted(
              component<AssignmentCardinalityComponent>(), 3)));
    static_assert(std::same_as<FluentAggregatedRecipe, PipedAggregatedRecipe>);

    const auto two_capacity_recipe =
        solution_manager<TwoCapacitySolutionManager>()
        | easylocal::cost::apply(
              TwoCapacityAggregator{},
              component<CapacityVariantA>(),
              component<CapacityVariantB>());
    using TwoCapacityConfigured =
        typename decltype(two_capacity_recipe)::service_type;
    static_assert(std::tuple_size_v<
        typename TwoCapacityConfigured::component_types> == 2);

    const auto hard_manager_recipe =
        solution_manager<TwoStageSolutionManager>()
        | easylocal::cost::apply(
              CapacityHardCost{},
              component<CapacityCostComponent>());
    const auto full_manager_recipe =
        solution_manager<TwoStageSolutionManager>()
        | easylocal::cost::hard_soft(
              easylocal::cost::apply(
                  CapacityHardCost{},
                  component<CapacityCostComponent>()),
              component<AssignmentCardinalityComponent>());

    const auto hard_neighborhood_recipe =
        neighborhood<CountingSingleMoveNeighborhood>(
            ReassignJobMove{.job = 1, .destination = 1},
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
            ReassignJobMove{.job = 1, .destination = 1},
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
            ReassignJobMove{.job = 1, .destination = 1},
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

    const AssignmentInstance instance{
        .demand = {4, 3, 2},
        .capacity = {5, 5},
    };
    const AssignmentSolution initial{
        .assignment = {0, 0, 1},
    };
    const ReassignJobMove relieving_move{
        .job = 1,
        .destination = 1,
    };

    // A single component: its value is the cost, with no weights to
    // configure.
    auto single_component_recipe =
        solution_manager<NoAggregateSolutionManager>()
        | component<AssignmentCardinalityComponent>();
    static_assert(!easylocal::config::configuration_provider<
                  decltype(single_component_recipe)>);
    const auto single_component_manager =
        single_component_recipe.construct(instance);
    ok &= expect(
        single_component_manager.evaluate(initial) == 3,
        "a single component is the cost");

    // A sum of arithmetic components: unit weights unless given, configurable
    // as cost.weights.
    auto sum_recipe =
        solution_manager<NoAggregateSolutionManager>()
        | easylocal::cost::sum(
              component<AssignmentCardinalityComponent>(),
              component<ColocatedCardinalityComponent>());

    const auto sum_configuration = easylocal::config::root(
        easylocal::config::named<"solver">(sum_recipe.configuration()));
    constexpr std::array sum_override{
        easylocal::config::text_override{
            "solver.cost.weights",
            "[4, 2]"},
    };
    const auto sum_override_result =
        easylocal::config::apply_overrides(sum_configuration, sum_override);
    ok &= expect(
        static_cast<bool>(sum_override_result),
        "the weights of a sum are exposed through configuration");

    const auto sum_manager = sum_recipe.construct(instance);
    ok &= expect(
        sum_manager.evaluate(initial) == 18,
        "a sum uses its configured weights");

    auto no_aggregate_recipe =
        solution_manager<NoAggregateSolutionManager>()
        | easylocal::cost::sum(easylocal::cost::weighted(
              component<AssignmentCardinalityComponent>(), 3));
    const auto aggregation_configuration = easylocal::config::root(
        easylocal::config::named<"solver">(
            no_aggregate_recipe.configuration()));
    constexpr std::array aggregation_override{
        easylocal::config::text_override{
            "solver.cost.weights",
            "[4]"},
    };
    const auto aggregation_override_result =
        easylocal::config::apply_overrides(
            aggregation_configuration,
            aggregation_override);
    ok &= expect(
        static_cast<bool>(aggregation_override_result),
        "the weights of a sum are exposed through the SolutionManager recipe configuration");

    const auto no_aggregate_manager = no_aggregate_recipe.construct(instance);
    ok &= expect(
        no_aggregate_manager.evaluate(initial) == 12,
        "a configured weighted sum is materialized without SM::aggregate");

    const auto stateless_component_manager =
        (solution_manager<NoAggregateSolutionManager>()
         | component<StatelessOffsetComponent>(std::size_t{5}))
            .construct(instance);
    ok &= expect(
        stateless_component_manager.evaluate(initial) == 8,
        "a cost component may be constructed from recipe arguments without an Instance");

    const auto two_capacity_manager = two_capacity_recipe.construct(instance);
    const auto two_capacity_cost = two_capacity_manager.evaluate(initial);

    ok &= expect(
        two_capacity_cost.get<0>() == 2 &&
            two_capacity_cost.get<1>() == 2,
        "distinct component subclasses are independently composed and evaluated");

    const auto stateless_delta_manager =
        (solution_manager<AssignmentSolutionManager>()
         | component<AssignmentCardinalityComponent>())
            .construct(instance);
    const auto stateless_delta_neighborhood =
        (neighborhood<ReassignJobNeighborhoodExplorer>()
         | delta<
               AssignmentCardinalityComponent,
               StatelessCardinalityDeltaEvaluator>())
            .construct(stateless_delta_manager);
    ok &= expect(
        std::tuple_size_v<
            typename decltype(stateless_delta_neighborhood)::delta_bindings_type> == 1,
        "a delta evaluator may be stateless and constructed without an Instance");

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
        hard_result.cost == HardCost{0, 0},
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
        full_result.cost.hard().get<0>() == 0 &&
            full_result.cost.hard().get<1>() == 0 &&
            full_result.cost.soft() == 3,
        "hard+soft stage uses a distinct local delta set on the same AssignmentSolution and ReassignJobMove types");
    ok &= expect(
        full_make_moves == 1,
        "hard+soft all-delta stage applies the accepted move exactly once");

    // A full-stage delta set is invalid when paired with a hard-only manager:
    // bind() uses the same predicate in a static_assert.
    using FullNHE = typename decltype(full_nhe)::service_type;
    static_assert(!easylocal::detail::all_delta_components_active_v<
        HardSM,
        FullNHE>);

    // A hierarchical hard projection must not evaluate soft components and
    // discard them afterwards. The configured manager exposes the hard
    // component prefix at compile time, so the hard view materializes only
    // those values.
    int soft_evaluations = 0;
    const auto zero_overhead_recipe =
        easylocal::solution_manager<TwoStageSolutionManager>()
        | easylocal::cost::hard_soft(
              easylocal::cost::apply(
                  CapacityHardCost{},
                  component<CapacityCostComponent>()),
              component<CountingSoftComponent>(std::ref(soft_evaluations)));
    const auto zero_overhead_full_sm = zero_overhead_recipe.construct(instance);
    using ZeroOverheadFullSM = decltype(zero_overhead_full_sm);
    static_assert(ZeroOverheadFullSM::has_hard_component_projection);
    static_assert(ZeroOverheadFullSM::hard_component_count == 1);

    easylocal::detail::hard_cost_layer<ZeroOverheadFullSM>
        zero_overhead_hard_sm{zero_overhead_full_sm};
    static_assert(std::tuple_size_v<
        typename decltype(zero_overhead_hard_sm)::component_types> == 1);

    const auto projected_cost = zero_overhead_hard_sm.evaluate(initial);
    ok &= expect(
        projected_cost == HardCost{2, 1},
        "hard projection preserves the hard aggregate");
    ok &= expect(
        soft_evaluations == 0,
        "hard projection does not evaluate soft components");

    const auto projected_component =
        zero_overhead_hard_sm.template evaluate_component<0>(initial);
    ok &= expect(
        projected_component.total_overload == 2 &&
            projected_component.overloaded_machines == 1,
        "hard projection keeps hard component fallback evaluation available");
    ok &= expect(
        soft_evaluations == 0,
        "hard component fallback does not evaluate soft components");

    return ok ? 0 : 1;
}
