#pragma once

// SolutionManager composition: the cost layer, symmetric to the
// NeighborhoodExplorer delta cost layer (EL3 CostComponent / DeltaCostComponent).
//
// cost_layer adds the cost components to the user SolutionManager and
// evaluates them into a tuple of component values; cost_layer_with_expression
// computes the cost from those values through the cost expression of the
// recipe. hard_cost_layer projects a hierarchical cost onto its hard branch for
// with_hard_cost(), evaluating only the hard components when the expression has
// a hard_soft root.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/helpers/detail/cost_expression.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/utils/detail/attributes.hpp>
#include <easylocal/utils/detail/meta.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::detail
{

// The composed SolutionManager (cost layer): solution semantics plus the
// cost. User SolutionManagers only model base_solution_manager.
template<class SM>
concept evaluable_solution_manager =
    easylocal::base_solution_manager<SM> &&
    requires(
        const SM& solution_manager,
        const typename SM::solution_type& solution)
    {
        typename SM::cost_type;

        {
            solution_manager.evaluate(solution)
        } -> std::same_as<typename SM::cost_type>;
    };

// A layer over a SolutionManager: it holds the layer below (Inner) and
// forwards to it what does not depend on the cost (the Input, validity,
// construction and solution identity); cost_layer, cost_layer_with_expression
// and hard_cost_layer add the cost. Base is the user's SolutionManager, which
// base() returns: Inner itself for the innermost layer.
template<class Inner, class Base = Inner>
class solution_manager_layer
{
public:
    using base_type = Base;
    using input_type = typename Inner::input_type;
    using solution_type = typename Inner::solution_type;

    explicit solution_manager_layer(Inner inner) : inner_{std::move(inner)} {}

    [[nodiscard]] base_type& base() noexcept
    {
        if constexpr (std::same_as<Inner, Base>)
            return inner_;
        else
            return inner_.base();
    }
    [[nodiscard]] const base_type& base() const noexcept
    {
        if constexpr (std::same_as<Inner, Base>)
            return inner_;
        else
            return inner_.base();
    }
    [[nodiscard]] const input_type& input() const noexcept
    {
        return inner_.input();
    }
    [[nodiscard]] bool is_valid(const solution_type& solution) const
        noexcept(noexcept(inner_.is_valid(solution)))
    {
        return inner_.is_valid(solution);
    }

    [[nodiscard]]
    solution_type initial_solution() const noexcept(noexcept(inner_.initial_solution()))
        requires has_initial_solution<Inner>
    {
        return inner_.initial_solution();
    }

    // The problem's solution identity, when it defines one (see
    // has_solution_hash); otherwise the solution type's own applies.
    [[nodiscard]]
    std::uint64_t hash(const solution_type& solution) const
        requires has_solution_hash_member<Inner>
    {
        return inner_.hash(solution);
    }

    [[nodiscard]]
    bool equal(const solution_type& lhs, const solution_type& rhs) const
        requires has_solution_equality_member<Inner>
    {
        return inner_.equal(lhs, rhs);
    }

    template<class RNG>
    [[nodiscard]]
    solution_type random_solution(RNG& rng) const
        noexcept(noexcept(inner_.random_solution(rng)))
        requires has_random_solution<Inner, RNG>
    {
        return inner_.random_solution(rng);
    }

    // The cost component of type Component of the innermost cost layer, for
    // the co-located deltas (delta<C>()) bound to this layer: every layer,
    // the hard projection included, reaches all the components.
    template<class Component, class Self>
    [[nodiscard]]
    auto& component(this Self&& self) noexcept
        requires requires(Inner& inner) { inner.template component<Component>(); }
    {
        return self.inner_.template component<Component>();
    }

protected:
    // The layer below, for what the derived layer adds on it.
    [[nodiscard]] Inner& inner() noexcept
    {
        return inner_;
    }
    [[nodiscard]] const Inner& inner() const noexcept
    {
        return inner_;
    }

private:
    Inner inner_;
};

// The innermost cost layer: the user's SolutionManager with the cost
// components of the recipe, built from its Input, which it evaluates into a
// tuple of component values, one per component, in the order of the recipe.
template<class BaseSM, class... ComponentSpecs>
class cost_layer : public solution_manager_layer<BaseSM>
{
public:
    using typename solution_manager_layer<BaseSM>::solution_type;
    using component_types = std::tuple<typename ComponentSpecs::component_type...>;
    using component_values_type = std::tuple<
        component_value_t<typename ComponentSpecs::component_type, solution_type>...>;

    static_assert(
        unique_types_v<typename ComponentSpecs::component_type...>,
        "a SolutionManager recipe may contain each component type at most once; "
        "the conflicting component type is shown in the template instantiation "
        "context");

    cost_layer(BaseSM base, const ComponentSpecs&... component_specs)
        : solution_manager_layer<BaseSM>{std::move(base)},
          components_{component_specs.construct(this->input())...}
    {
        static_assert(
            sizeof...(ComponentSpecs) > 0,
            "a SolutionManager needs at least one cost component");
    }

    [[nodiscard]]
    component_values_type evaluate_components(const solution_type& solution) const
    {
        return std::apply(
            [&](const auto&... component) {
                return component_values_type{component.evaluate(solution)...};
            },
            components_);
    }

    template<std::size_t Index>
    [[nodiscard]]
    std::tuple_element_t<Index, component_values_type> evaluate_component(
        const solution_type& solution) const
    {
        return std::get<Index>(components_).evaluate(solution);
    }

    template<class Component, class Self>
    [[nodiscard]]
    auto& component(this Self&& self) noexcept
    {
        static_assert(
            tuple_contains_type_v<Component, component_types>,
            "the requested cost component is not active in this SolutionManager");
        return std::get<tuple_type_index_v<Component, component_types>>(self.components_);
    }

private:
    std::tuple<typename ComponentSpecs::component_type...> components_;
};

// The cost layer with its cost expression: the cost is computed from the
// component values by the expression tree, whose leaves are exactly the
// components of the inner layer, in the same order.
template<class InnerSM, class Expression>
class cost_layer_with_expression
    : public solution_manager_layer<InnerSM, typename InnerSM::base_type>
{
    using layer = solution_manager_layer<InnerSM, typename InnerSM::base_type>;

public:
    using typename layer::solution_type;
    using component_types = typename InnerSM::component_types;
    using component_values_type = typename InnerSM::component_values_type;
    using cost_type = typename Expression::cost_type;

    // A hard_soft root exposes its hard components, the leading ones.
    static constexpr bool has_hard_component_projection = requires
    {
        Expression::hard_leaf_count;
    };
    static constexpr std::size_t hard_component_count = []
    {
        if constexpr (has_hard_component_projection)
            return Expression::hard_leaf_count;
        else
            return std::tuple_size_v<component_values_type>;
    }();

    using hard_component_types = tuple_prefix_t<
        component_types,
        hard_component_count>;
    using hard_component_values_type = tuple_prefix_t<
        component_values_type,
        hard_component_count>;

    cost_layer_with_expression(InnerSM inner, Expression expression)
        : layer{std::move(inner)}, expression_{std::move(expression)}
    {
    }

    [[nodiscard]]
    component_values_type evaluate_components(const solution_type& solution) const
    {
        return this->inner().evaluate_components(solution);
    }

    [[nodiscard]]
    hard_component_values_type evaluate_hard_components(
        const solution_type& solution) const
        requires has_hard_component_projection
    {
        return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
            return hard_component_values_type{
                this->inner().template evaluate_component<Indices>(solution)...,
            };
        }(std::make_index_sequence<hard_component_count>{});
    }

    template<std::size_t Index>
    [[nodiscard]]
    std::tuple_element_t<Index, component_values_type> evaluate_component(
        const solution_type& solution) const
    {
        return this->inner().template evaluate_component<Index>(solution);
    }

    template<std::size_t Index>
    [[nodiscard]]
    std::tuple_element_t<Index, hard_component_values_type> evaluate_hard_component(
        const solution_type& solution) const
        requires(has_hard_component_projection && Index < hard_component_count)
    {
        return this->inner().template evaluate_component<Index>(solution);
    }

    [[nodiscard]]
    cost_type cost_from_components(const component_values_type& values) const
    {
        return expression_.template evaluate<0>(values);
    }

    [[nodiscard]]
    auto hard_cost_from_components(const hard_component_values_type& values) const
        requires has_hard_component_projection
    {
        return expression_.hard_cost(values);
    }

    [[nodiscard]]
    cost_type evaluate(const solution_type& solution) const
    {
        return cost_from_components(evaluate_components(solution));
    }

    [[nodiscard]]
    const Expression& cost_expression() const noexcept
    {
        return expression_;
    }

private:
    EASYLOCAL_NO_UNIQUE_ADDRESS Expression expression_;
};

// A composed SolutionManager whose cost is hierarchical (hard and soft), the
// cost with_hard_cost() requires.
template<class SM>
concept hierarchical_solution_manager = requires {
    typename SM::cost_type;
} && cost::hierarchical_cost<typename SM::cost_type>;

// The user's SolutionManager under SM, what the hard layer's base() returns
// (the neighborhood explorers are built from it): SM::base() when SM is a
// composed layer, otherwise SM itself.
template<class SM>
struct hard_layer_base
{
    using type = SM;
};

template<class SM>
    requires requires(SM& solution_manager) { solution_manager.base(); }
struct hard_layer_base<SM>
{
    using type = std::remove_cvref_t<decltype(std::declval<SM&>().base())>;
};

template<class SM>
using hard_layer_base_t = typename hard_layer_base<SM>::type;

// The hard-cost projection of a SolutionManager with a hierarchical cost, for
// with_hard_cost(): the same solutions, with the hard branch as cost.
template<class SM>
class hard_cost_layer_base : public solution_manager_layer<SM, hard_layer_base_t<SM>>
{
    using layer = solution_manager_layer<SM, hard_layer_base_t<SM>>;

public:
    using typename layer::solution_type;
    using full_cost_type = typename SM::cost_type;
    using cost_type = typename full_cost_type::hard_cost_type;

    explicit hard_cost_layer_base(SM solution_manager)
        : layer{std::move(solution_manager)}
    {
    }

    [[nodiscard]]
    cost_type evaluate(const solution_type& solution) const
    {
        return this->inner().evaluate(solution).hard();
    }

    // The order of the hard costs, when the root of the cost expression gives
    // one (cost::approximately): the hard stage compares as the full cost does.
    [[nodiscard]]
    auto cost_expression() const
        requires requires(const SM& inner) { inner.cost_expression().hard_semantics(); }
    {
        return this->inner().cost_expression().hard_semantics();
    }
};

// The hard-cost projection; when SM has cost components, it evaluates only the
// hard ones (the leading leaves of a hard_soft expression) where it can.
template<class SM, bool = requires {
    typename SM::component_types;
    typename SM::component_values_type;
}>
class hard_cost_layer;

template<class SM>
class hard_cost_layer<SM, false>
    : public hard_cost_layer_base<SM>
{
public:
    using hard_cost_layer_base<SM>::hard_cost_layer_base;
};

template<class SM>
class hard_cost_layer<SM, true>
    : public hard_cost_layer_base<SM>
{
public:
    // The C++ base class; base_type in the other layers is the user's
    // SolutionManager.
    using layer_base = hard_cost_layer_base<SM>;
    using typename layer_base::cost_type;
    using typename layer_base::solution_type;

    static constexpr bool projected_components = requires
    {
        typename SM::hard_component_types;
        typename SM::hard_component_values_type;
        requires SM::has_hard_component_projection;
    };

    using component_types = std::conditional_t<
        projected_components,
        typename SM::hard_component_types,
        typename SM::component_types>;
    using component_values_type = std::conditional_t<
        projected_components,
        typename SM::hard_component_values_type,
        typename SM::component_values_type>;
    using projection_source_component_types = typename SM::component_types;

    using layer_base::layer_base;

    [[nodiscard]]
    component_values_type evaluate_components(const solution_type& solution) const
    {
        if constexpr (projected_components)
            return this->inner().evaluate_hard_components(solution);
        else
            return this->inner().evaluate_components(solution);
    }

    template<std::size_t Index>
    [[nodiscard]]
    std::tuple_element_t<Index, component_values_type> evaluate_component(
        const solution_type& solution) const
    {
        if constexpr (projected_components)
            return this->inner().template evaluate_hard_component<Index>(solution);
        else
            return this->inner().template evaluate_component<Index>(solution);
    }

    [[nodiscard]]
    cost_type cost_from_components(const component_values_type& values) const
    {
        if constexpr (projected_components)
            return this->inner().hard_cost_from_components(values);
        else
            return this->inner().cost_from_components(values).hard();
    }

    [[nodiscard]]
    cost_type evaluate(const solution_type& solution) const
    {
        return cost_from_components(evaluate_components(solution));
    }
};

// The recipe of a hard-cost projection: it builds the full SolutionManager
// from SMSpec and wraps it in a hard_cost_layer; its parameters are SMSpec's.
template<class SMSpec>
class hard_cost_layer_spec
{
public:
    using service_type = hard_cost_layer<
        typename SMSpec::service_type>;

    explicit hard_cost_layer_spec(SMSpec spec)
        : spec_{std::move(spec)}
    {
    }

    template<class Dependency>
    static constexpr bool constructible_from =
        SMSpec::template constructible_from<Dependency>;

    template<class Dependency>
        requires constructible_from<Dependency>
    [[nodiscard]]
    service_type construct(Dependency& dependency) const
    {
        return service_type{spec_.construct(dependency)};
    }

    [[nodiscard]]
    config::parameter_set configuration() &
        requires config::detail::configuration_provider<SMSpec>
    {
        return spec_.configuration();
    }

    [[nodiscard]]
    config::parameter_set configuration() const&
        requires config::detail::configuration_provider<const SMSpec>
    {
        return spec_.configuration();
    }

    // A temporary has no configuration: the set would refer to it after it is
    // gone. Configure the object that will run.
    config::parameter_set configuration() const&& = delete;

private:
    SMSpec spec_;
};

} // namespace easylocal::detail
