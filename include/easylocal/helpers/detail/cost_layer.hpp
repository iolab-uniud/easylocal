#pragma once

#include <easylocal/config/tree.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/utils/detail/meta.hpp>
#include <easylocal/utils/logging.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

// SolutionManager composition: the cost layer, symmetric to the
// NeighborhoodExplorer delta cost layer (EL3 CostComponent / DeltaCostComponent).
//
// cost_layer adds the cost components to the user SolutionManager and
// evaluates them into a tuple of component values; cost_layer_with_aggregator
// computes the cost from those values through the attached aggregator (or an
// implicit unit-weight weighted_sum). hard_cost_layer projects a hierarchical
// cost onto its hard branch for TwoStage, evaluating only the hard component
// prefix.
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

template<class Component, class... StoredArgs>
class component_spec
{
public:
    using component_type = Component;

    explicit component_spec(StoredArgs... args)
        : args_{std::move(args)...}
    {
    }

    template<class Instance>
    [[nodiscard]]
    auto construct(const Instance& instance) const -> Component
    {
        return std::apply(
            [&](const auto&... args) -> Component {
                if constexpr (std::constructible_from<
                                  Component,
                                  const Instance&,
                                  const StoredArgs&...>)
                {
                    return Component{instance, args...};
                }
                else
                {
                    static_assert(
                        std::constructible_from<Component, const StoredArgs&...>,
                        "a cost component must be constructible either from the "
                        "bound Instance followed by its recipe arguments or from "
                        "its recipe arguments alone");
                    return Component{args...};
                }
            },
            args_);
    }

    [[nodiscard]]
    auto args() && noexcept -> std::tuple<StoredArgs...>&&
    {
        return std::move(args_);
    }

private:
    std::tuple<StoredArgs...> args_;
};

template<class Component, class Solution>
using component_value_t = std::remove_cvref_t<decltype(
    std::declval<const Component&>().evaluate(
        std::declval<const Solution&>()))>;

template<class BaseSM, class... ComponentSpecs>
class cost_layer
{
public:
    using base_type = BaseSM;
    using input_type = typename BaseSM::input_type;
    using solution_type = typename BaseSM::solution_type;
    using component_types = std::tuple<typename ComponentSpecs::component_type...>;
    using component_values_type = std::tuple<
        component_value_t<
            typename ComponentSpecs::component_type,
            solution_type>...>;

    static_assert(
        unique_types_v<typename ComponentSpecs::component_type...>,
        "a SolutionManager recipe may contain each component type at most once; "
        "the conflicting component type is shown in the template instantiation "
        "context");

    cost_layer(
        BaseSM base,
        const ComponentSpecs&... component_specs)
        : base_{std::move(base)},
          components_{component_specs.construct(base_.input())...}
    {
        static_assert(sizeof...(ComponentSpecs) > 0,
            "an aggregated SolutionManager needs at least one cost component");
    }

    [[nodiscard]] auto base() noexcept -> BaseSM& { return base_; }
    [[nodiscard]] auto base() const noexcept -> const BaseSM& { return base_; }
    [[nodiscard]] auto input() const noexcept -> const input_type& { return base_.input(); }
    [[nodiscard]] auto is_valid(const solution_type& solution) const noexcept(noexcept(base_.is_valid(solution))) -> bool { return base_.is_valid(solution); }

    [[nodiscard]]
    auto initial_solution() const noexcept(noexcept(base_.initial_solution()))
        -> solution_type
        requires has_initial_solution<BaseSM>
    {
        return base_.initial_solution();
    }

    template<class RNG>
    [[nodiscard]]
    auto random_solution(RNG& rng) const noexcept(noexcept(base_.random_solution(rng)))
        -> solution_type
        requires has_random_solution<BaseSM, RNG>
    {
        return base_.random_solution(rng);
    }

    [[nodiscard]]
    auto evaluate_components(const solution_type& solution) const
        -> component_values_type
    {
        return std::apply(
            [&](const auto&... component) {
                return component_values_type{component.evaluate(solution)...};
            },
            components_);
    }

    template<std::size_t Index>
    [[nodiscard]]
    auto evaluate_component(const solution_type& solution) const
        -> std::tuple_element_t<Index, component_values_type>
    {
        return std::get<Index>(components_).evaluate(solution);
    }

    template<class Component>
    [[nodiscard]]
    auto component() noexcept -> Component&
    {
        static_assert(
            tuple_contains_type_v<Component, component_types>,
            "the requested cost component is not active in this SolutionManager");
        return std::get<tuple_type_index_v<Component, component_types>>(components_);
    }

    template<class Component>
    [[nodiscard]]
    auto component() const noexcept -> const Component&
    {
        static_assert(
            tuple_contains_type_v<Component, component_types>,
            "the requested cost component is not active in this SolutionManager");
        return std::get<tuple_type_index_v<Component, component_types>>(components_);
    }

private:
    BaseSM base_;
    std::tuple<typename ComponentSpecs::component_type...> components_;
};

struct no_implicit_aggregator
{
};

// The aggregator used when a recipe does not attach one explicitly:
// - a single component: its value is the cost (identity), silently;
// - several arithmetic components: a unit-weight weighted_sum, with a warning
//   because the weights are a guess (they stay configurable as cost.weights);
// - otherwise none: an explicit aggregator is required, so domain value types
//   never need to support weighting just to be aggregated.
struct identity_aggregator
{
    template<class Value>
    [[nodiscard]]
    constexpr auto operator()(const Value& value) const -> Value
    {
        return value;
    }
};

template<class Tuple>
struct implicit_aggregator_traits
{
    static constexpr bool available = false;
    static constexpr bool warns = false;
    using type = no_implicit_aggregator;
};

template<class Value>
struct implicit_aggregator_traits<std::tuple<Value>>
{
    static constexpr bool available = true;
    static constexpr bool warns = false;
    using type = identity_aggregator;

    [[nodiscard]]
    static constexpr auto make() -> type
    {
        return {};
    }
};

template<class First, class Second, class... Rest>
    requires cost::arithmetic<First> && cost::arithmetic<Second> &&
             (cost::arithmetic<Rest> && ...)
struct implicit_aggregator_traits<std::tuple<First, Second, Rest...>>
{
    static constexpr bool available = true;
    static constexpr bool warns = true;
    using weight_type = std::common_type_t<First, Second, Rest...>;
    using type = cost::weighted_sum<weight_type, 2 + sizeof...(Rest)>;

    [[nodiscard]]
    static constexpr auto make() -> type
    {
        return []<std::size_t... Indices>(std::index_sequence<Indices...>) {
            return type{((void)Indices, weight_type{1})...};
        }(std::make_index_sequence<2 + sizeof...(Rest)>{});
    }
};

template<class Aggregator>
void warn_implicit_aggregator()
{
    static bool warned = false;
    if (!warned) {
        warned = true;
        logging::emit(
            logging::level::warning,
            "cost.aggregation",
            "no cost aggregator was specified for several cost components; "
            "using an implicit unit-weight weighted_sum. Override cost.weights "
            "or add `| aggregator(...)` / `.with_aggregator(...)` to make the "
            "aggregation explicit.",
            logging::origin::framework);
    }
}

template<class Aggregator>
class aggregator_spec
{
public:
    using aggregator_type = Aggregator;

    explicit aggregator_spec(Aggregator aggregator)
        : aggregator_{std::move(aggregator)}
    {
    }

    [[nodiscard]]
    auto get() & noexcept -> Aggregator&
    {
        return aggregator_;
    }

    [[nodiscard]]
    auto get() const & noexcept -> const Aggregator&
    {
        return aggregator_;
    }

    [[nodiscard]]
    auto get() && noexcept -> Aggregator&&
    {
        return std::move(aggregator_);
    }

private:
    [[no_unique_address]] Aggregator aggregator_;
};

template<class InnerSM, class Aggregator>
class cost_layer_with_aggregator
{
public:
    using base_type = typename InnerSM::base_type;
    using input_type = typename InnerSM::input_type;
    using solution_type = typename InnerSM::solution_type;
    using component_types = typename InnerSM::component_types;
    using component_values_type = typename InnerSM::component_values_type;

private:
    template<class Tuple>
    struct aggregate_result;

    template<class... Values>
    struct aggregate_result<std::tuple<Values...>>
    {
        using type = std::remove_cvref_t<decltype(
            std::declval<const Aggregator&>()(
                std::declval<const Values&>()...))>;
    };

public:
    using cost_type = typename aggregate_result<component_values_type>::type;

    static constexpr bool hierarchical_cost = requires
    {
        typename cost_type::hard_cost_type;
        typename cost_type::soft_cost_type;
    };

private:
    template<std::size_t... Indices>
    [[nodiscard]]
    static consteval auto hard_prefix_matches(std::index_sequence<Indices...>)
        -> bool
    {
        if constexpr (!hierarchical_cost)
        {
            return false;
        }
        else
        {
            return cost::hard_projection<
                Aggregator,
                typename cost_type::hard_cost_type,
                std::tuple_element_t<Indices, component_values_type>...>;
        }
    }

    template<std::size_t Count = 1>
    [[nodiscard]]
    static consteval auto find_hard_prefix() -> std::size_t
    {
        constexpr auto count = std::tuple_size_v<component_values_type>;
        if constexpr (!hierarchical_cost || Count >= count)
        {
            return count;
        }
        else if constexpr (hard_prefix_matches(
                               std::make_index_sequence<Count>{}))
        {
            return Count;
        }
        else
        {
            return find_hard_prefix<Count + 1>();
        }
    }

public:
    static constexpr std::size_t hard_component_count = find_hard_prefix<>();
    static constexpr bool has_hard_component_projection =
        hierarchical_cost &&
        hard_component_count < std::tuple_size_v<component_values_type>;

    using hard_component_types = tuple_prefix_t<
        component_types,
        hard_component_count>;
    using hard_component_values_type = tuple_prefix_t<
        component_values_type,
        hard_component_count>;

    cost_layer_with_aggregator(InnerSM inner, Aggregator aggregator)
        : inner_{std::move(inner)}, aggregator_{std::move(aggregator)}
    {
    }

    [[nodiscard]] auto base() noexcept -> base_type& { return inner_.base(); }
    [[nodiscard]] auto base() const noexcept -> const base_type& { return inner_.base(); }
    [[nodiscard]] auto input() const noexcept -> const input_type& { return inner_.input(); }
    [[nodiscard]] auto is_valid(const solution_type& solution) const noexcept(noexcept(inner_.is_valid(solution))) -> bool { return inner_.is_valid(solution); }

    [[nodiscard]]
    auto initial_solution() const noexcept(noexcept(inner_.initial_solution()))
        -> solution_type
        requires has_initial_solution<InnerSM>
    {
        return inner_.initial_solution();
    }

    template<class RNG>
    [[nodiscard]]
    auto random_solution(RNG& rng) const noexcept(noexcept(inner_.random_solution(rng)))
        -> solution_type
        requires has_random_solution<InnerSM, RNG>
    {
        return inner_.random_solution(rng);
    }

    [[nodiscard]]
    auto evaluate_components(const solution_type& solution) const
        -> component_values_type
    {
        return inner_.evaluate_components(solution);
    }

    [[nodiscard]]
    auto evaluate_hard_components(const solution_type& solution) const
        -> hard_component_values_type
        requires has_hard_component_projection
    {
        return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
            return hard_component_values_type{
                inner_.template evaluate_component<Indices>(solution)...,
            };
        }(std::make_index_sequence<hard_component_count>{});
    }

    template<std::size_t Index>
    [[nodiscard]]
    auto evaluate_component(const solution_type& solution) const
        -> std::tuple_element_t<Index, component_values_type>
    {
        return inner_.template evaluate_component<Index>(solution);
    }

    template<std::size_t Index>
    [[nodiscard]]
    auto evaluate_hard_component(const solution_type& solution) const
        -> std::tuple_element_t<Index, hard_component_values_type>
        requires (has_hard_component_projection && Index < hard_component_count)
    {
        return inner_.template evaluate_component<Index>(solution);
    }

    [[nodiscard]]
    auto cost_from_components(const component_values_type& values) const -> cost_type
    {
        return std::apply(
            [&](const auto&... value) -> cost_type {
                return aggregator_(value...);
            },
            values);
    }

    [[nodiscard]]
    auto hard_cost_from_components(const hard_component_values_type& values) const
        requires has_hard_component_projection
    {
        return std::apply(
            [&](const auto&... value) {
                return aggregator_.hard(value...);
            },
            values);
    }

    [[nodiscard]]
    auto evaluate(const solution_type& solution) const -> cost_type
    {
        return cost_from_components(evaluate_components(solution));
    }

    template<class Component>
    [[nodiscard]]
    auto component() noexcept -> Component&
    {
        return inner_.template component<Component>();
    }

    template<class Component>
    [[nodiscard]]
    auto component() const noexcept -> const Component&
    {
        return inner_.template component<Component>();
    }

    [[nodiscard]]
    auto aggregator() noexcept -> Aggregator& { return aggregator_; }
    [[nodiscard]]
    auto aggregator() const noexcept -> const Aggregator& { return aggregator_; }

private:
    InnerSM inner_;
    [[no_unique_address]] Aggregator aggregator_;
};

template<class SM>
concept hierarchical_solution_manager =
    requires { typename SM::cost_type; } &&
    cost::hierarchical_type<typename SM::cost_type>;

template<class SM>
class hard_cost_layer_base
{
public:
    using underlying_type = SM;
    using input_type = typename SM::input_type;
    using solution_type = typename SM::solution_type;
    using full_cost_type = typename SM::cost_type;
    using cost_type = typename full_cost_type::hard_cost_type;

    explicit hard_cost_layer_base(SM solution_manager)
        : solution_manager_{std::move(solution_manager)}
    {
    }

    [[nodiscard]]
    auto input() const noexcept -> const input_type&
    {
        return solution_manager_.input();
    }

    [[nodiscard]]
    auto is_valid(const solution_type& solution) const
        noexcept(noexcept(solution_manager_.is_valid(solution))) -> bool
    {
        return solution_manager_.is_valid(solution);
    }

    [[nodiscard]]
    auto initial_solution() const -> solution_type
        requires has_initial_solution<SM>
    {
        return solution_manager_.initial_solution();
    }

    template<class RNG>
    [[nodiscard]]
    auto random_solution(RNG& rng) const -> solution_type
        requires has_random_solution<SM, RNG>
    {
        return solution_manager_.random_solution(rng);
    }

    [[nodiscard]]
    auto evaluate(const solution_type& solution) const -> cost_type
    {
        return solution_manager_.evaluate(solution).hard();
    }

    [[nodiscard]]
    auto base() noexcept -> decltype(auto)
    {
        if constexpr (requires { solution_manager_.base(); })
            return solution_manager_.base();
        else
            return (solution_manager_);
    }

    [[nodiscard]]
    auto base() const noexcept -> decltype(auto)
    {
        if constexpr (requires { solution_manager_.base(); })
            return solution_manager_.base();
        else
            return (solution_manager_);
    }

protected:
    SM solution_manager_;
};

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
    using base_type = hard_cost_layer_base<SM>;
    using typename base_type::cost_type;
    using typename base_type::solution_type;

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

    using base_type::base_type;

    [[nodiscard]]
    auto evaluate_components(const solution_type& solution) const
        -> component_values_type
    {
        if constexpr (projected_components)
            return this->solution_manager_.evaluate_hard_components(solution);
        else
            return this->solution_manager_.evaluate_components(solution);
    }

    template<std::size_t Index>
    [[nodiscard]]
    auto evaluate_component(const solution_type& solution) const
        -> std::tuple_element_t<Index, component_values_type>
    {
        if constexpr (projected_components)
            return this->solution_manager_.template evaluate_hard_component<Index>(
                solution);
        else
            return this->solution_manager_.template evaluate_component<Index>(
                solution);
    }

    [[nodiscard]]
    auto cost_from_components(const component_values_type& values) const -> cost_type
    {
        if constexpr (projected_components)
            return this->solution_manager_.hard_cost_from_components(values);
        else
            return this->solution_manager_.cost_from_components(values).hard();
    }

    [[nodiscard]]
    auto evaluate(const solution_type& solution) const -> cost_type
    {
        return cost_from_components(evaluate_components(solution));
    }
};

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
    auto construct(Dependency& dependency) const -> service_type
    {
        return service_type{spec_.construct(dependency)};
    }

    [[nodiscard]]
    auto configuration()
        requires config::configuration_provider<SMSpec>
    {
        return spec_.configuration();
    }

    [[nodiscard]]
    auto configuration() const
        requires config::configuration_provider<const SMSpec>
    {
        return spec_.configuration();
    }

private:
    SMSpec spec_;
};

} // namespace easylocal::detail
