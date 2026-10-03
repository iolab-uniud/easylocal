#pragma once

// How a move or a solution is evaluated for the runners: the cost together
// with its per-component values (materialized_evaluation, candidate_evaluation),
// and the compile-time checks that the delta bindings of a neighborhood match
// the cost components of the SolutionManager.

#include <easylocal/helpers/detail/neighborhood_recipe.hpp>
#include <easylocal/helpers/detail/solution_manager_recipe.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::detail
{

template<class SM>
concept component_evaluation_solution_manager =
    requires(
        const SM& solution_manager,
        const typename SM::solution_type& solution,
        const typename SM::component_values_type& values)
    {
        typename SM::input_type;
        typename SM::solution_type;
        typename SM::cost_type;
        typename SM::component_types;
        typename SM::component_values_type;

        requires (
            std::tuple_size_v<typename SM::component_types> ==
            std::tuple_size_v<typename SM::component_values_type>);

        {
            solution_manager.input()
        } -> std::same_as<const typename SM::input_type&>;

        {
            solution_manager.evaluate(solution)
        } -> std::same_as<typename SM::cost_type>;

        {
            solution_manager.evaluate_components(solution)
        } -> std::same_as<typename SM::component_values_type>;

        {
            solution_manager.cost_from_components(values)
        } -> std::same_as<typename SM::cost_type>;
    };

template<class SM, bool = component_evaluation_solution_manager<SM>>
struct evaluation_metadata
{
    using component_types = std::tuple<>;
    using component_values_type = std::tuple<>;
};

template<class SM>
struct evaluation_metadata<SM, true>
{
    using component_types = typename SM::component_types;
    using component_values_type = typename SM::component_values_type;
};

template<class Cost, class ComponentValues>
class materialized_evaluation
{
public:
    materialized_evaluation(ComponentValues component_values, Cost cost)
        : component_values_{std::move(component_values)},
          cost_{std::move(cost)}
    {
    }

    [[nodiscard]]
    const Cost& cost() const noexcept
    {
        return cost_;
    }

    [[nodiscard]]
    const ComponentValues& component_values() const noexcept
    {
        return component_values_;
    }

private:
    ComponentValues component_values_;
    Cost cost_;
};

template<class Solution, class Move, class Evaluation, bool Materialized>
class candidate_evaluation;

template<class Solution, class Move, class Evaluation>
class candidate_evaluation<Solution, Move, Evaluation, false>
{
public:
    candidate_evaluation(Evaluation evaluation, Move move)
        : evaluation_{std::move(evaluation)},
          move_{std::move(move)}
    {
    }

    [[nodiscard]]
    decltype(auto) cost() const noexcept
    {
        return evaluation_.cost();
    }

    [[nodiscard]]
    const Move& move() const noexcept
    {
        return move_;
    }

    [[nodiscard]]
    Evaluation& evaluation() & noexcept
    {
        return evaluation_;
    }

private:
    Evaluation evaluation_;
    Move move_;
};

template<class Solution, class Move, class Evaluation>
class candidate_evaluation<Solution, Move, Evaluation, true>
{
public:
    candidate_evaluation(Evaluation evaluation, Solution solution)
        : evaluation_{std::move(evaluation)},
          solution_{std::move(solution)}
    {
    }

    [[nodiscard]]
    decltype(auto) cost() const noexcept
    {
        return evaluation_.cost();
    }

    [[nodiscard]]
    Evaluation& evaluation() & noexcept
    {
        return evaluation_;
    }

    [[nodiscard]]
    Solution& solution() & noexcept
    {
        return solution_;
    }

private:
    Evaluation evaluation_;
    Solution solution_;
};

template<class NHE, class = void>
struct neighborhood_delta_metadata
{
    using delta_bindings_type = std::tuple<>;
};

template<class NHE>
struct neighborhood_delta_metadata<
    NHE,
    std::void_t<typename NHE::delta_bindings_type>>
{
    using delta_bindings_type = typename NHE::delta_bindings_type;
};

template<class NHE>
using neighborhood_delta_bindings_t =
    typename neighborhood_delta_metadata<NHE>::delta_bindings_type;

template<class Binding, class Component, class Move, class Solution, class Value>
concept delta_binding_for =
    std::same_as<typename Binding::component_type, Component> &&
    requires(
        const Binding& binding,
        const Solution& solution,
        const Move& move,
        const Value& value)
    {
        {
            binding.apply(value, solution, move)
        } -> std::same_as<Value>;
    };

template<class SM, class Binding>
inline constexpr bool delta_component_active_v =
    component_evaluation_solution_manager<SM> &&
    tuple_contains_type_v<
        typename Binding::component_type,
        typename SM::component_types>;

// A component of the recipe that a projection leaves out, such as a soft
// component in the hard-cost projection of TwoStage's first stage: its delta
// belongs to the recipe but is not used by the projected SolutionManager.
template<class SM, class Binding>
consteval bool delta_component_projected_out()
{
    if constexpr (requires { typename SM::projection_source_component_types; })
    {
        return !delta_component_active_v<SM, Binding>
            && tuple_contains_type_v<
                typename Binding::component_type,
                typename SM::projection_source_component_types>;
    }
    else
    {
        return false;
    }
}

template<class SM, class NHE, class Binding>
consteval bool delta_binding_compatible()
{
    if constexpr (!delta_component_active_v<SM, Binding>)
    {
        return false;
    }
    else
    {
        using component_type = typename Binding::component_type;
        constexpr auto component_index =
            tuple_type_index_v<component_type, typename SM::component_types>;
        using value_type = std::tuple_element_t<
            component_index,
            typename SM::component_values_type>;

        return delta_binding_for<
            Binding,
            component_type,
            typename NHE::move_type,
            typename SM::solution_type,
            value_type>;
    }
}

template<class SM, class NHE, std::size_t... Indices>
consteval bool all_delta_components_active_impl(std::index_sequence<Indices...>)
{
    using bindings = neighborhood_delta_bindings_t<NHE>;
    return (
        delta_component_active_v<SM, std::tuple_element_t<Indices, bindings>> &&
        ...);
}

template<class SM, class NHE>
inline constexpr bool all_delta_components_active_v =
    all_delta_components_active_impl<SM, NHE>(
        std::make_index_sequence<
            std::tuple_size_v<neighborhood_delta_bindings_t<NHE>>>{});

template<class SM, class NHE, std::size_t... Indices>
consteval bool all_delta_bindings_compatible_impl(std::index_sequence<Indices...>)
{
    using bindings = neighborhood_delta_bindings_t<NHE>;
    return (
        delta_binding_compatible<
            SM,
            NHE,
            std::tuple_element_t<Indices, bindings>>() &&
        ...);
}

template<class SM, class NHE>
inline constexpr bool all_delta_bindings_compatible_v =
    all_delta_bindings_compatible_impl<SM, NHE>(
        std::make_index_sequence<
            std::tuple_size_v<neighborhood_delta_bindings_t<NHE>>>{});

template<class SM, class NHE, class Binding>
consteval bool validate_delta_binding()
{
    static_assert(
        delta_component_active_v<SM, Binding>
            || delta_component_projected_out<SM, Binding>(),
        "attached delta names a component that is not active in the bound "
        "SolutionManager recipe; the offending component and delta evaluator "
        "types are shown in the template instantiation context");

    if constexpr (delta_component_active_v<SM, Binding>)
    {
        static_assert(
            delta_binding_compatible<SM, NHE, Binding>(),
            "attached delta evaluator is incompatible with the bound component "
            "value, Solution, or Move type; the offending component and delta "
            "evaluator types are shown in the template instantiation context");
    }

    return true;
}

template<class SM, class NHE, std::size_t... Indices>
consteval bool validate_delta_bindings_impl(std::index_sequence<Indices...>)
{
    using bindings = neighborhood_delta_bindings_t<NHE>;
    return (validate_delta_binding<
                SM,
                NHE,
                std::tuple_element_t<Indices, bindings>>() &&
            ...);
}

template<class SM, class NHE>
consteval bool validate_delta_bindings()
{
    return validate_delta_bindings_impl<SM, NHE>(
        std::make_index_sequence<
            std::tuple_size_v<neighborhood_delta_bindings_t<NHE>>>{});
}

template<class Component, std::size_t Count>
consteval bool has_unique_delta_binding()
{
    static_assert(
        Count <= 1,
        "at most one delta evaluator may be attached to a component type; "
        "the offending component type is shown in the template instantiation "
        "context");
    return Count == 1;
}

template<class SM, class NHE>
class evaluation_facility
{
private:
    static constexpr bool component_aware =
        component_evaluation_solution_manager<SM>;

    using metadata = evaluation_metadata<SM>;
    using component_types = typename metadata::component_types;
    using component_values_type = typename metadata::component_values_type;
    using delta_bindings_type = neighborhood_delta_bindings_t<NHE>;

    template<std::size_t ComponentIndex, std::size_t... DeltaIndices>
    [[nodiscard]]
    static consteval std::size_t matching_delta_count_impl(
        std::index_sequence<DeltaIndices...>)
    {
        using component_type =
            std::tuple_element_t<ComponentIndex, component_types>;

        return (
            std::size_t{0} + ... +
            static_cast<std::size_t>(std::same_as<
                typename std::tuple_element_t<
                    DeltaIndices,
                    delta_bindings_type>::component_type,
                component_type>));
    }

    template<std::size_t ComponentIndex>
    [[nodiscard]]
    static consteval std::size_t matching_delta_count()
    {
        if constexpr (!component_aware)
        {
            return 0;
        }
        else
        {
            return matching_delta_count_impl<ComponentIndex>(
                std::make_index_sequence<
                    std::tuple_size_v<delta_bindings_type>>{});
        }
    }

    template<std::size_t ComponentIndex>
    [[nodiscard]]
    static consteval bool has_delta()
    {
        using component_type =
            std::tuple_element_t<ComponentIndex, component_types>;
        constexpr auto count = matching_delta_count<ComponentIndex>();
        return has_unique_delta_binding<component_type, count>();
    }

    template<std::size_t... ComponentIndices>
    [[nodiscard]]
    static consteval bool needs_materialized_candidate_impl(
        std::index_sequence<ComponentIndices...>)
    {
        return ((!has_delta<ComponentIndices>()) || ...);
    }

    [[nodiscard]]
    static consteval bool needs_materialized_candidate()
    {
        if constexpr (!component_aware)
        {
            return true;
        }
        else
        {
            return needs_materialized_candidate_impl(
                std::make_index_sequence<
                    std::tuple_size_v<component_types>>{});
        }
    }

    template<std::size_t ComponentIndex, std::size_t DeltaIndex = 0>
    [[nodiscard]]
    std::tuple_element_t<ComponentIndex, component_values_type>
    evaluate_component_for_move(
        const typename SM::solution_type& current_solution,
        const component_values_type& current_values,
        const typename NHE::move_type& move,
        const typename SM::solution_type* materialized_candidate) const
    {
        using component_type =
            std::tuple_element_t<ComponentIndex, component_types>;
        using value_type =
            std::tuple_element_t<ComponentIndex, component_values_type>;

        if constexpr (DeltaIndex < std::tuple_size_v<delta_bindings_type>)
        {
            using binding_type =
                std::tuple_element_t<DeltaIndex, delta_bindings_type>;

            if constexpr (std::same_as<
                              typename binding_type::component_type,
                              component_type>)
            {
                static_assert(
                    delta_binding_for<
                        binding_type,
                        component_type,
                        typename NHE::move_type,
                        typename SM::solution_type,
                        value_type>,
                    "attached delta evaluator is incompatible with its component, "
                    "Solution, or Move");

                return std::get<DeltaIndex>(neighborhood_.delta_bindings())
                    .apply(
                        std::get<ComponentIndex>(current_values),
                        current_solution,
                        move);
            }
            else
            {
                return evaluate_component_for_move<
                    ComponentIndex,
                    DeltaIndex + 1>(
                    current_solution,
                    current_values,
                    move,
                    materialized_candidate);
            }
        }
        else
        {
            assert(materialized_candidate != nullptr);
            return solution_manager_.template evaluate_component<ComponentIndex>(
                *materialized_candidate);
        }
    }

    template<std::size_t... ComponentIndices>
    [[nodiscard]]
    component_values_type evaluate_components_for_move(
        const typename SM::solution_type& current_solution,
        const component_values_type& current_values,
        const typename NHE::move_type& move,
        const typename SM::solution_type* materialized_candidate,
        std::index_sequence<ComponentIndices...>) const
    {
        return component_values_type{
            evaluate_component_for_move<ComponentIndices>(
                current_solution,
                current_values,
                move,
                materialized_candidate)...,
        };
    }

public:
    using solution_type = typename SM::solution_type;
    using cost_type = typename SM::cost_type;
    using evaluation_type =
        materialized_evaluation<cost_type, component_values_type>;

    using move_type = typename NHE::move_type;
    using candidate_type = candidate_evaluation<
        solution_type,
        move_type,
        evaluation_type,
        needs_materialized_candidate()>;

    evaluation_facility(
        const SM& solution_manager,
        const NHE& neighborhood) noexcept
        : solution_manager_{solution_manager},
          neighborhood_{neighborhood}
    {
        static_assert(validate_delta_bindings<SM, NHE>());
    }

    [[nodiscard]]
    evaluation_type evaluate(const solution_type& solution) const
    {
        assert(solution_manager_.is_valid(solution));

        if constexpr (component_aware)
        {
            auto component_values =
                solution_manager_.evaluate_components(solution);
            auto cost = solution_manager_.cost_from_components(component_values);

            return evaluation_type{
                std::move(component_values),
                std::move(cost),
            };
        }
        else
        {
            return evaluation_type{
                {},
                solution_manager_.evaluate(solution),
            };
        }
    }

    [[nodiscard]]
    candidate_type evaluate_move(
        const solution_type& current_solution,
        const evaluation_type& current,
        const move_type& move) const
    {
        assert(solution_manager_.is_valid(current_solution));
        assert(neighborhood_.is_valid(current_solution, move));

        if constexpr (needs_materialized_candidate())
        {
            auto candidate_solution = current_solution;
            neighborhood_.make_move(candidate_solution, move);
            assert(solution_manager_.is_valid(candidate_solution));

            if constexpr (component_aware)
            {
                auto component_values = evaluate_components_for_move(
                    current_solution,
                    current.component_values(),
                    move,
                    &candidate_solution,
                    std::make_index_sequence<
                        std::tuple_size_v<component_types>>{});
                auto cost = solution_manager_.cost_from_components(component_values);

                return candidate_type{
                    evaluation_type{
                        std::move(component_values),
                        std::move(cost),
                    },
                    std::move(candidate_solution),
                };
            }
            else
            {
                return candidate_type{
                    evaluation_type{
                        {},
                        solution_manager_.evaluate(candidate_solution),
                    },
                    std::move(candidate_solution),
                };
            }
        }
        else
        {
            static_assert(
                component_aware,
                "an all-delta candidate requires a component-aware "
                "SolutionManager");

            auto component_values = evaluate_components_for_move(
                current_solution,
                current.component_values(),
                move,
                nullptr,
                std::make_index_sequence<
                    std::tuple_size_v<component_types>>{});
            auto cost = solution_manager_.cost_from_components(component_values);

            return candidate_type{
                evaluation_type{
                    std::move(component_values),
                    std::move(cost),
                },
                move,
            };
        }
    }

    void commit(
        solution_type& solution,
        evaluation_type& current,
        candidate_type&& candidate) const
    {
        if constexpr (needs_materialized_candidate())
        {
            solution = std::move(candidate.solution());
        }
        else
        {
            assert(solution_manager_.is_valid(solution));
            assert(neighborhood_.is_valid(solution, candidate.move()));
            neighborhood_.make_move(solution, candidate.move());
        }

        assert(solution_manager_.is_valid(solution));
        current = std::move(candidate.evaluation());
    }

private:
    const SM& solution_manager_;
    const NHE& neighborhood_;
};

} // namespace easylocal::detail
