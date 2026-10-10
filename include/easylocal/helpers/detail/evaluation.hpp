#pragma once

// How a move or a solution is evaluated for the runners: the cost together
// with its per-component values (materialized_evaluation, candidate_evaluation),
// and the compile-time checks that the delta bindings of a neighborhood match
// the cost components of the SolutionManager.

#include <easylocal/helpers/detail/neighborhood_recipe.hpp>
#include <easylocal/helpers/detail/solution_manager_recipe.hpp>
#include <easylocal/utils/detail/expensive_assert.hpp>

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
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

// A scratch solution, made at its first use and reused: a copy assignment keeps
// its storage. A copy of it starts without one. (Not an optional, which GCC 15
// at -O3 reports as maybe uninitialized.)
template<class Solution>
class scratch_solution
{
public:
    scratch_solution() = default;
    scratch_solution(const scratch_solution&) noexcept {}
    scratch_solution& operator=(const scratch_solution&) noexcept
    {
        return *this;
    }
    scratch_solution(scratch_solution&&) noexcept = default;
    scratch_solution& operator=(scratch_solution&&) noexcept = default;
    ~scratch_solution() = default;

    // The scratch solution, a copy of from.
    Solution& assign(const Solution& from)
    {
        if (solution_ != nullptr)
            *solution_ = from;
        else
            solution_ = std::make_unique<Solution>(from);
        return *solution_;
    }

    // The scratch solution, or nullptr before the first assign().
    [[nodiscard]]
    Solution* get() const noexcept
    {
        return solution_.get();
    }

private:
    std::unique_ptr<Solution> solution_;
};

// The evaluation of a move: its cost (with the component values), the move,
// which commit() applies to the solution, and the number of the scratch
// solution it was evaluated on (0: none).
template<class Move, class Evaluation>
class candidate_evaluation
{
public:
    candidate_evaluation(Evaluation evaluation, Move move, const std::size_t scratch = 0)
        : evaluation_{std::move(evaluation)}, move_{std::move(move)}, scratch_{scratch}
    {
    }

    [[nodiscard]]
    std::size_t scratch() const noexcept
    {
        return scratch_;
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
    std::size_t scratch_;
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

// A delta that covers some moves only, which a neighborhood union forwards
// from the children that have one: it gives the new value of the component, or
// nothing for a move whose child has no delta, which is then evaluated on the
// candidate solution.
template<class Binding, class Component, class Move, class Solution, class Value>
concept partial_delta_binding_for =
    std::same_as<typename Binding::component_type, Component>
    && requires(
        const Binding& binding,
        const Solution& solution,
        const Move& move,
        const Value& value) {
           {
               binding.try_apply(value, solution, move)
           } -> std::same_as<std::optional<Value>>;
       };

template<class SM, class Binding>
inline constexpr bool delta_component_active_v =
    component_evaluation_solution_manager<SM> &&
    tuple_contains_type_v<
        typename Binding::component_type,
        typename SM::component_types>;

// A component of the recipe that a projection leaves out, such as a soft
// component in the hard-cost projection of with_hard_cost(): its delta
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
                   value_type>
            || partial_delta_binding_for<
                Binding,
                component_type,
                typename NHE::move_type,
                typename SM::solution_type,
                value_type>;
    }
}

template<class SM, class NHE, class Binding>
consteval bool validate_delta_binding()
{
    static_assert(
        delta_component_active_v<SM, Binding>
            || delta_component_projected_out<SM, Binding>(),
        "attached delta names a component that is not active in the bound "
        "SolutionManager recipe; the offending component and delta cost component "
        "types are shown in the template instantiation context");

    if constexpr (delta_component_active_v<SM, Binding>)
    {
        static_assert(
            delta_binding_compatible<SM, NHE, Binding>(),
            "attached delta cost component is incompatible with the bound component "
            "value, Solution, or Move type: delta_evaluate(const Solution&, const Move&) "
            "returns the change of the component value; the offending component and "
            "delta cost component types are shown in the template instantiation "
            "context");
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
        "at most one delta cost component may be attached to a component type; "
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

    // Whether the delta of the component covers every move: a partial one,
    // which a neighborhood union forwards from some of its children, leaves
    // the moves of the others to the full evaluation.
    template<std::size_t ComponentIndex, std::size_t DeltaIndex = 0>
    [[nodiscard]]
    static consteval bool has_total_delta()
    {
        using component_type = std::tuple_element_t<ComponentIndex, component_types>;

        if constexpr (DeltaIndex >= std::tuple_size_v<delta_bindings_type>)
        {
            return false;
        }
        else
        {
            using binding_type = std::tuple_element_t<DeltaIndex, delta_bindings_type>;

            if constexpr (std::same_as<
                              typename binding_type::component_type,
                              component_type>)
            {
                return delta_binding_for<
                    binding_type,
                    component_type,
                    typename NHE::move_type,
                    typename SM::solution_type,
                    std::tuple_element_t<ComponentIndex, component_values_type>>;
            }
            else
            {
                return has_total_delta<ComponentIndex, DeltaIndex + 1>();
            }
        }
    }

    template<std::size_t... ComponentIndices>
    [[nodiscard]]
    static consteval bool may_materialize_candidate_impl(
        std::index_sequence<ComponentIndices...>)
    {
        return ((!has_total_delta<ComponentIndices>()) || ...);
    }

    // Whether a move may be evaluated on a copy of the solution with the move
    // made: when a component has no delta, or one that covers some moves only.
    [[nodiscard]]
    static consteval bool may_materialize_candidate()
    {
        if constexpr (!component_aware)
        {
            return true;
        }
        else
        {
            return may_materialize_candidate_impl(
                std::make_index_sequence<std::tuple_size_v<component_types>>{});
        }
    }

    // The value of one component after the move: from its delta, total or
    // partial, or from the candidate solution, which provide_candidate() makes
    // on the first component that needs it.
    template<std::size_t ComponentIndex, std::size_t DeltaIndex = 0, class Candidate>
    [[nodiscard]]
    std::tuple_element_t<ComponentIndex, component_values_type>
    evaluate_component_for_move(
        const typename SM::solution_type& current_solution,
        const component_values_type& current_values,
        const typename NHE::move_type& move,
        Candidate& provide_candidate) const
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
                        value_type>
                        || partial_delta_binding_for<
                            binding_type,
                            component_type,
                            typename NHE::move_type,
                            typename SM::solution_type,
                            value_type>,
                    "attached delta cost component is incompatible with its component, "
                    "Solution, or Move");

                // A neighborhood union returns its bindings by value: the
                // reference is kept alive here, not on the temporary.
                auto&& bindings = neighborhood_.delta_bindings();
                const auto& binding = std::get<DeltaIndex>(bindings);

                if constexpr (has_total_delta<ComponentIndex>())
                {
                    return binding.apply(
                        std::get<ComponentIndex>(current_values),
                        current_solution,
                        move);
                }
                else
                {
                    auto value = binding.try_apply(
                        std::get<ComponentIndex>(current_values),
                        current_solution,
                        move);
                    if (value)
                        return *std::move(value);

                    return solution_manager_.template evaluate_component<ComponentIndex>(
                        provide_candidate());
                }
            }
            else
            {
                return evaluate_component_for_move<ComponentIndex, DeltaIndex + 1>(
                    current_solution,
                    current_values,
                    move,
                    provide_candidate);
            }
        }
        else
        {
            return solution_manager_.template evaluate_component<ComponentIndex>(
                provide_candidate());
        }
    }

    template<class Candidate, std::size_t... ComponentIndices>
    [[nodiscard]]
    component_values_type evaluate_components_for_move(
        const typename SM::solution_type& current_solution,
        const component_values_type& current_values,
        const typename NHE::move_type& move,
        Candidate& provide_candidate,
        std::index_sequence<ComponentIndices...>) const
    {
        return component_values_type{
            evaluate_component_for_move<ComponentIndices>(
                current_solution,
                current_values,
                move,
                provide_candidate)...,
        };
    }

    // The scratch solution with the move made, reused from one move to the
    // next: a copy assignment keeps its storage.
    [[nodiscard]]
    const typename SM::solution_type* materialize(
        const typename SM::solution_type& current_solution,
        const typename NHE::move_type& move) const
    {
        auto& candidate_solution = scratch_.assign(current_solution);
        scratch_number_ = ++scratches_;
        neighborhood_.make_move(candidate_solution, move);
        // A whole-solution check per evaluated move: opt-in.
        EASYLOCAL_EXPENSIVE_ASSERT(solution_manager_.is_valid(candidate_solution));
        return std::addressof(candidate_solution);
    }

public:
    using solution_type = typename SM::solution_type;
    using cost_type = typename SM::cost_type;
    using evaluation_type =
        materialized_evaluation<cost_type, component_values_type>;

    using move_type = typename NHE::move_type;
    using candidate_type = candidate_evaluation<move_type, evaluation_type>;
    // Whether a move may be evaluated on a copy of the solution with the move
    // made: when a component has no delta, or one that covers some moves only.
    static constexpr bool materializes_candidates = may_materialize_candidate();

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
        assert(neighborhood_.is_valid(current_solution, move));

        if constexpr (!component_aware)
        {
            const auto* candidate_solution = materialize(current_solution, move);

            return candidate_type{
                evaluation_type{
                    {},
                    solution_manager_.evaluate(*candidate_solution),
                },
                move,
                scratch_number_,
            };
        }
        else
        {
            // The candidate solution is made by the first component that needs
            // one and kept for the others of the same move; a move all the
            // deltas cover never makes one.
            const solution_type* candidate_solution = nullptr;
            auto provide_candidate = [&]() -> const solution_type& {
                if (candidate_solution == nullptr)
                    candidate_solution = materialize(current_solution, move);
                return *candidate_solution;
            };

            auto component_values = evaluate_components_for_move(
                current_solution,
                current.component_values(),
                move,
                provide_candidate,
                std::make_index_sequence<std::tuple_size_v<component_types>>{});
            auto cost = solution_manager_.cost_from_components(component_values);

            return candidate_type{
                evaluation_type{
                    std::move(component_values),
                    std::move(cost),
                },
                move,
                candidate_solution != nullptr ? scratch_number_ : std::size_t{0},
            };
        }
    }

    void commit(
        solution_type& solution,
        evaluation_type& current,
        candidate_type&& candidate) const
    {
        // The candidate of the last move evaluated on the scratch solution is
        // swapped in; another one (kept while others were evaluated) has its
        // move made again, which keeps no solution per candidate.
        if constexpr (may_materialize_candidate())
        {
            if (scratch_.get() != nullptr && candidate.scratch() != 0
                && candidate.scratch() == scratch_number_)
            {
                using std::swap;
                swap(solution, *scratch_.get());
                scratch_number_ = 0;
                current = std::move(candidate.evaluation());
                return;
            }
        }
        assert(neighborhood_.is_valid(solution, candidate.move()));
        neighborhood_.make_move(solution, candidate.move());
        assert(solution_manager_.is_valid(solution));
        current = std::move(candidate.evaluation());
#ifdef EASYLOCAL_VERIFY_DELTAS
        verify_deltas(solution, current);
#endif
    }

private:
    // Whether a component value from the deltas agrees with the full
    // evaluation: within 1e-9 relative for a floating-point value.
    template<class Value>
    [[nodiscard]]
    static bool same_component_value(const Value& incremental, const Value& full)
    {
        if constexpr (std::floating_point<Value>)
        {
            const auto scale = (std::
                    max)({Value{1}, incremental < Value{} ? -incremental : incremental, full < Value{} ? -full : full});
            const auto difference = incremental - full;
            return (difference < Value{} ? -difference : difference)
                <= Value{1e-9} * scale;
        }
        else if constexpr (std::equality_comparable<Value>)
            return static_cast<bool>(incremental == full);
        else
            return true;
    }

    // With EASYLOCAL_VERIFY_DELTAS, the values of the components after a
    // commit, which the deltas computed, against a full evaluation of the
    // solution: a disagreement names the component (by its position, from 1)
    // and stops the program.
    void verify_deltas(
        const solution_type& solution,
        const evaluation_type& current) const
    {
        if constexpr (component_aware)
        {
            const auto full = evaluate(solution);
            [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
                (
                    [&] {
                        if (!same_component_value(
                                std::get<Indices>(current.component_values()),
                                std::get<Indices>(full.component_values())))
                        {
                            std::fprintf(
                                stderr,
                                "EASYLOCAL_VERIFY_DELTAS: the delta of cost component "
                                "#%zu disagrees with its full evaluation after a move\n",
                                Indices + 1);
                            std::abort();
                        }
                    }(),
                    ...);
            }(std::make_index_sequence<std::tuple_size_v<component_types>>{});
        }
    }

    const SM& solution_manager_;
    const NHE& neighborhood_;
    // The candidate solution of the last move evaluated on one, its number
    // (0 once committed) and the count of the moves evaluated on it.
    mutable scratch_solution<solution_type> scratch_;
    mutable std::size_t scratch_number_{};
    mutable std::size_t scratches_{};
};

} // namespace easylocal::detail
