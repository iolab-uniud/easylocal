#pragma once

#include <easylocal/aggregation.hpp>
#include <easylocal/config/tree.hpp>
#include <easylocal/detail/solution_manager_concepts.hpp>

#include <concepts>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::detail
{

template<class T, class... Ts>
inline constexpr bool type_in_pack_v = (std::same_as<T, Ts> || ...);

template<class... Ts>
struct unique_types : std::true_type
{
};

template<class T, class... Rest>
struct unique_types<T, Rest...>
    : std::bool_constant<
          !type_in_pack_v<T, Rest...> && unique_types<Rest...>::value>
{
};

template<class... Ts>
inline constexpr bool unique_types_v = unique_types<Ts...>::value;

template<class T, class Tuple>
struct tuple_contains_type;

template<class T, class... Ts>
struct tuple_contains_type<T, std::tuple<Ts...>>
    : std::bool_constant<type_in_pack_v<T, Ts...>>
{
};

template<class T, class Tuple>
inline constexpr bool tuple_contains_type_v = tuple_contains_type<T, Tuple>::value;

template<class T, class Tuple>
struct tuple_type_index;

template<class T, class... Rest>
struct tuple_type_index<T, std::tuple<T, Rest...>>
    : std::integral_constant<std::size_t, 0>
{
};

template<class T, class First, class... Rest>
struct tuple_type_index<T, std::tuple<First, Rest...>>
    : std::integral_constant<
          std::size_t,
          1 + tuple_type_index<T, std::tuple<Rest...>>::value>
{
};

template<class T, class Tuple>
inline constexpr std::size_t tuple_type_index_v = tuple_type_index<T, Tuple>::value;

template<class Tuple, std::size_t... Indices>
[[nodiscard]]
auto tuple_prefix_type_impl(std::index_sequence<Indices...>)
    -> std::tuple<std::tuple_element_t<Indices, Tuple>...>;

template<class Tuple, std::size_t Count>
using tuple_prefix_t = decltype(
    tuple_prefix_type_impl<Tuple>(std::make_index_sequence<Count>{}));

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

template<class Component, class DeltaEvaluator>
class delta_binding
{
public:
    using component_type = Component;
    using evaluator_type = DeltaEvaluator;

    explicit delta_binding(DeltaEvaluator evaluator)
        : evaluator_{std::move(evaluator)}
    {
    }

    [[nodiscard]]
    auto evaluator() const noexcept -> const DeltaEvaluator&
    {
        return evaluator_;
    }

    template<class Value, class Solution, class Move>
        requires requires(
            const Value& value,
            const DeltaEvaluator& evaluator,
            const Solution& solution,
            const Move& move)
        {
            {
                value + evaluator.delta_evaluate(solution, move)
            } -> std::same_as<Value>;
        }
    [[nodiscard]]
    auto apply(
        const Value& value,
        const Solution& solution,
        const Move& move) const -> Value
    {
        return value + evaluator_.delta_evaluate(solution, move);
    }

private:
    [[no_unique_address]] DeltaEvaluator evaluator_;
};

template<class Component, class DeltaEvaluator, class... StoredArgs>
class delta_spec
{
public:
    using component_type = Component;
    using evaluator_type = DeltaEvaluator;
    using binding_type = delta_binding<Component, DeltaEvaluator>;

    explicit delta_spec(StoredArgs... args)
        : args_{std::move(args)...}
    {
    }

    template<class Dependency>
    [[nodiscard]]
    auto construct(Dependency& dependency) const -> binding_type
    {
        return std::apply(
            [&](const auto&... args) -> binding_type {
                using instance_type = typename std::remove_cvref_t<Dependency>::instance_type;
                if constexpr (std::constructible_from<
                                  DeltaEvaluator,
                                  const instance_type&,
                                  const StoredArgs&...>)
                {
                    return binding_type{
                        DeltaEvaluator{dependency.instance(), args...},
                    };
                }
                else
                {
                    static_assert(
                        std::constructible_from<
                            DeltaEvaluator,
                            const StoredArgs&...>,
                        "a delta evaluator must be constructible either from the "
                        "bound Instance followed by its recipe arguments or from "
                        "its recipe arguments alone");
                    return binding_type{DeltaEvaluator{args...}};
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

template<class Component>
class colocated_delta_binding
{
public:
    using component_type = Component;

    explicit colocated_delta_binding(const Component& component) noexcept
        : component_{component}
    {
    }

    template<class Value, class Solution, class Move>
        requires requires(
            const Value& value,
            const Component& component,
            const Solution& solution,
            const Move& move)
        {
            {
                value + component.delta_evaluate(solution, move)
            } -> std::same_as<Value>;
        }
    [[nodiscard]]
    auto apply(
        const Value& value,
        const Solution& solution,
        const Move& move) const -> Value
    {
        return value + component_.get().delta_evaluate(solution, move);
    }

private:
    std::reference_wrapper<const Component> component_;
};

template<class Component>
class colocated_delta_spec
{
public:
    using component_type = Component;
    using evaluator_type = Component;
    using binding_type = colocated_delta_binding<Component>;

    template<class Dependency>
    [[nodiscard]]
    auto construct(Dependency& dependency) const -> binding_type
    {
        return binding_type{dependency.template component<Component>()};
    }
};

template<class Component, class Solution>
using component_value_t = std::remove_cvref_t<decltype(
    std::declval<const Component&>().evaluate(
        std::declval<const Solution&>()))>;

template<class BaseSM, class... ComponentSpecs>
class component_solution_manager
{
public:
    using base_type = BaseSM;
    using instance_type = typename BaseSM::instance_type;
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

    component_solution_manager(
        BaseSM base,
        const ComponentSpecs&... component_specs)
        : base_{std::move(base)},
          components_{component_specs.construct(base_.instance())...}
    {
        static_assert(sizeof...(ComponentSpecs) > 0,
            "an aggregated SolutionManager needs at least one cost component");
    }

    [[nodiscard]] auto base() noexcept -> BaseSM& { return base_; }
    [[nodiscard]] auto base() const noexcept -> const BaseSM& { return base_; }
    [[nodiscard]] auto instance() const noexcept -> const instance_type& { return base_.instance(); }
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

template<class... Values>
using implicit_unit_weight_t = std::common_type_t<
    decltype(int{1} * std::declval<const Values&>())...>;

template<class Weight, class... Values>
using implicit_weighted_sum_result_t = decltype(
    (... + (std::declval<Weight>() * std::declval<const Values&>())));

template<class Tuple, class = void>
struct implicit_weighted_sum_traits
{
    static constexpr bool available = false;
    using type = no_implicit_aggregator;
};

template<class... Values>
struct implicit_weighted_sum_traits<
    std::tuple<Values...>,
    std::void_t<
        implicit_unit_weight_t<Values...>,
        implicit_weighted_sum_result_t<
            implicit_unit_weight_t<Values...>,
            Values...>>>
{
    static constexpr bool available = sizeof...(Values) > 0;
    using weight_type = implicit_unit_weight_t<Values...>;
    using type = aggregation::weighted_sum<weight_type, sizeof...(Values)>;

    [[nodiscard]]
    static constexpr auto make() -> type
    {
        return []<std::size_t... Indices>(std::index_sequence<Indices...>) {
            return type{((void)Indices, weight_type{1})...};
        }(std::make_index_sequence<sizeof...(Values)>{});
    }
};

template<class Tuple>
using implicit_aggregator_traits = implicit_weighted_sum_traits<Tuple>;

template<class Aggregator>
void warn_implicit_aggregator()
{
    static const bool warned = [] {
        std::fputs(
            "EasyLocal warning: no cost aggregator was specified; using an "
            "implicit unit-weight weighted_sum. Override cost.weights or add "
            "`| aggregator(...)` / `.with_aggregator(...)` to make the "
            "aggregation explicit.\n",
            stderr);
        return true;
    }();
    (void)warned;
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
class aggregated_solution_manager
{
public:
    using base_type = typename InnerSM::base_type;
    using instance_type = typename InnerSM::instance_type;
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
            return requires(
                const Aggregator& aggregator,
                const std::tuple_element_t<Indices, component_values_type>&...
                    values)
            {
                {
                    aggregator.hard(values...)
                } -> std::same_as<typename cost_type::hard_cost_type>;
            };
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

    aggregated_solution_manager(InnerSM inner, Aggregator aggregator)
        : inner_{std::move(inner)}, aggregator_{std::move(aggregator)}
    {
    }

    [[nodiscard]] auto base() noexcept -> base_type& { return inner_.base(); }
    [[nodiscard]] auto base() const noexcept -> const base_type& { return inner_.base(); }
    [[nodiscard]] auto instance() const noexcept -> const instance_type& { return inner_.instance(); }
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

template<class SMSpec, class AggregatorSpec>
class solution_manager_with_aggregator_recipe
{
public:
    using inner_spec_type = SMSpec;
    using aggregator_spec_type = AggregatorSpec;
    using aggregator_type = typename AggregatorSpec::aggregator_type;
    using service_type = aggregated_solution_manager<
        typename SMSpec::component_service_type,
        aggregator_type>;

    solution_manager_with_aggregator_recipe(
        SMSpec inner,
        AggregatorSpec aggregator)
        : inner_{std::move(inner)}, aggregator_{std::move(aggregator)}
    {
    }

    template<class Dependency>
    static constexpr bool constructible_from =
        SMSpec::template constructible_from<Dependency>;

    template<class Dependency>
    [[nodiscard]]
    auto construct(Dependency& dependency) const -> service_type
    {
        return service_type{
            inner_.construct_components(dependency),
            aggregator_.get()};
    }

    [[nodiscard]]
    auto configuration()
        requires config::configuration_provider<aggregator_type>
    {
        return aggregator_.get().configuration();
    }

    [[nodiscard]]
    auto configuration() const
        requires config::configuration_provider<const aggregator_type>
    {
        return aggregator_.get().configuration();
    }

private:
    SMSpec inner_;
    AggregatorSpec aggregator_;
};

template<class BaseSM, class Instance, class Tuple>
struct base_solution_manager_constructible;

template<class BaseSM, class Instance, class... Args>
struct base_solution_manager_constructible<BaseSM, Instance, std::tuple<Args...>>
    : std::bool_constant<
          std::constructible_from<BaseSM, const Instance&, const Args&...>>
{
};

template<class BaseSM, class Instance, class Tuple>
inline constexpr bool base_solution_manager_constructible_v =
    base_solution_manager_constructible<BaseSM, Instance, Tuple>::value;

template<class BaseSM, class BaseArgsTuple, class... ComponentSpecs>
class solution_manager_recipe
{
public:
    static_assert(
        base_solution_manager<BaseSM>,
        "a SolutionManager must expose instance_type, solution_type, "
        "instance() -> const instance_type&, and is_valid(solution)");

    using base_type = BaseSM;
    using component_service_type = component_solution_manager<
        BaseSM,
        ComponentSpecs...>;
    using component_types = std::tuple<typename ComponentSpecs::component_type...>;
    using component_values_type = typename component_service_type::component_values_type;
    using implicit_aggregator_traits_type =
        implicit_aggregator_traits<component_values_type>;
    static constexpr bool has_implicit_aggregator =
        sizeof...(ComponentSpecs) > 0 &&
        implicit_aggregator_traits_type::available;
    using implicit_aggregator_type = std::conditional_t<
        has_implicit_aggregator,
        typename implicit_aggregator_traits_type::type,
        no_implicit_aggregator>;
    using service_type = std::conditional_t<
        sizeof...(ComponentSpecs) == 0,
        BaseSM,
        std::conditional_t<
            has_implicit_aggregator,
            aggregated_solution_manager<
                component_service_type,
                implicit_aggregator_type>,
            component_service_type>>;

    static_assert(
        unique_types_v<typename ComponentSpecs::component_type...>,
        "a SolutionManager recipe may contain each component type at most once; "
        "the conflicting component type is shown in the template instantiation "
        "context");

    explicit solution_manager_recipe(BaseArgsTuple base_args)
        requires (sizeof...(ComponentSpecs) == 0)
        : base_args_{std::move(base_args)},
          implicit_aggregator_{make_implicit_aggregator()}
    {
    }

    solution_manager_recipe(
        BaseArgsTuple base_args,
        std::tuple<ComponentSpecs...> component_specs)
        : base_args_{std::move(base_args)},
          component_specs_{std::move(component_specs)},
          implicit_aggregator_{make_implicit_aggregator()}
    {
    }

    template<class Component, class... Args>
    [[nodiscard]]
    auto with_component(Args&&... args) const &
    {
        static_assert(
            !type_in_pack_v<Component, typename ComponentSpecs::component_type...>,
            "a SolutionManager recipe may contain each component type at most once; "
            "the conflicting component type is shown in the template instantiation "
            "context");

        using spec_type = component_spec<Component, std::decay_t<Args>...>;
        using result_type = solution_manager_recipe<
            BaseSM,
            BaseArgsTuple,
            ComponentSpecs...,
            spec_type>;

        return result_type{
            base_args_,
            std::tuple_cat(
                component_specs_,
                std::tuple{spec_type{std::forward<Args>(args)...}}),
        };
    }

    template<class Component, class... Args>
    [[nodiscard]]
    auto with_component(Args&&... args) &&
    {
        static_assert(
            !type_in_pack_v<Component, typename ComponentSpecs::component_type...>,
            "a SolutionManager recipe may contain each component type at most once; "
            "the conflicting component type is shown in the template instantiation "
            "context");

        using spec_type = component_spec<Component, std::decay_t<Args>...>;
        using result_type = solution_manager_recipe<
            BaseSM,
            BaseArgsTuple,
            ComponentSpecs...,
            spec_type>;

        return result_type{
            std::move(base_args_),
            std::tuple_cat(
                std::move(component_specs_),
                std::tuple{spec_type{std::forward<Args>(args)...}}),
        };
    }

    template<class Aggregator>
    [[nodiscard]]
    auto with_aggregator(Aggregator aggregator) const &
    {
        using spec_type = aggregator_spec<std::remove_cvref_t<Aggregator>>;
        return solution_manager_with_aggregator_recipe<
            solution_manager_recipe,
            spec_type>{
                *this,
                spec_type{std::move(aggregator)},
            };
    }

    template<class Aggregator>
    [[nodiscard]]
    auto with_aggregator(Aggregator aggregator) &&
    {
        using spec_type = aggregator_spec<std::remove_cvref_t<Aggregator>>;
        return solution_manager_with_aggregator_recipe<
            solution_manager_recipe,
            spec_type>{
                std::move(*this),
                spec_type{std::move(aggregator)},
            };
    }

    [[nodiscard]]
    auto configuration()
        requires (
            has_implicit_aggregator &&
            config::configuration_provider<implicit_aggregator_type>)
    {
        return implicit_aggregator_.configuration();
    }

    [[nodiscard]]
    auto configuration() const
        requires (
            has_implicit_aggregator &&
            config::configuration_provider<const implicit_aggregator_type>)
    {
        return implicit_aggregator_.configuration();
    }

    template<class Dependency>
    static constexpr bool constructible_from =
        std::same_as<
            std::remove_cvref_t<Dependency>,
            typename BaseSM::instance_type>;

    [[nodiscard]]
    auto construct_components(const typename BaseSM::instance_type& instance) const
        -> component_service_type
    {
        static_assert(
            sizeof...(ComponentSpecs) > 0,
            "an explicit aggregator requires at least one cost component");
        static_assert(
            base_solution_manager_constructible_v<
                BaseSM,
                typename BaseSM::instance_type,
                BaseArgsTuple>,
            "a SolutionManager derived from solution_manager_base must inherit "
            "the base constructors; did you forget `using solution_manager_base::solution_manager_base;`?");

        auto base = std::apply(
            [&](const auto&... args) {
                return BaseSM{instance, args...};
            },
            base_args_);

        return std::apply(
            [&](const auto&... specs) {
                return component_service_type{std::move(base), specs...};
            },
            component_specs_);
    }

    [[nodiscard]]
    auto construct(const typename BaseSM::instance_type& instance) const
        -> service_type
    {
        static_assert(
            base_solution_manager_constructible_v<
                BaseSM,
                typename BaseSM::instance_type,
                BaseArgsTuple>,
            "a SolutionManager derived from solution_manager_base must inherit "
            "the base constructors; did you forget `using solution_manager_base::solution_manager_base;`?");

        auto base = std::apply(
            [&](const auto&... args) {
                return BaseSM{instance, args...};
            },
            base_args_);

        if constexpr (sizeof...(ComponentSpecs) == 0)
        {
            return base;
        }
        else
        {
            auto components = std::apply(
                [&](const auto&... specs) {
                    return component_service_type{std::move(base), specs...};
                },
                component_specs_);

            if constexpr (has_implicit_aggregator)
            {
                warn_implicit_aggregator<implicit_aggregator_type>();
                return service_type{
                    std::move(components),
                    implicit_aggregator_};
            }
            else
            {
                return components;
            }
        }
    }

private:
    [[nodiscard]]
    static constexpr auto make_implicit_aggregator()
        -> implicit_aggregator_type
    {
        if constexpr (has_implicit_aggregator)
        {
            return implicit_aggregator_traits_type::make();
        }
        else
        {
            return {};
        }
    }

    BaseArgsTuple base_args_;
    std::tuple<ComponentSpecs...> component_specs_;
    [[no_unique_address]] implicit_aggregator_type implicit_aggregator_;
};

template<class BaseNHE, class... DeltaSpecs>
class configured_neighborhood : public BaseNHE
{
public:
    using base_type = BaseNHE;
    using delta_bindings_type = std::tuple<typename DeltaSpecs::binding_type...>;

    static_assert(
        unique_types_v<typename DeltaSpecs::component_type...>,
        "a neighborhood recipe may attach at most one delta evaluator "
        "to each component type; the conflicting component type is shown in "
        "the template instantiation context");

    configured_neighborhood(
        BaseNHE base,
        typename DeltaSpecs::binding_type... bindings)
        : BaseNHE{std::move(base)},
          delta_bindings_{std::move(bindings)...}
    {
    }

    [[nodiscard]]
    auto delta_bindings() const noexcept -> const delta_bindings_type&
    {
        return delta_bindings_;
    }

private:
    [[no_unique_address]] delta_bindings_type delta_bindings_;
};

template<class BaseNHE, class... DeltaSpecs>
using neighborhood_service_t = std::conditional_t<
    sizeof...(DeltaSpecs) == 0,
    BaseNHE,
    configured_neighborhood<BaseNHE, DeltaSpecs...>>;

template<class BaseNHE, class Dependency, class... Args>
consteval auto base_neighborhood_constructible_from_args() -> bool
{
    if constexpr (requires(Dependency& dependency) { dependency.base(); })
    {
        return std::constructible_from<
                   BaseNHE,
                   decltype(std::declval<Dependency&>().base()),
                   const Args&...> ||
               std::constructible_from<BaseNHE, Dependency&, const Args&...>;
    }
    else
    {
        return std::constructible_from<BaseNHE, Dependency&, const Args&...>;
    }
}

template<class BaseNHE, class Dependency, class Tuple>
struct base_neighborhood_constructible;

template<class BaseNHE, class Dependency, class... Args>
struct base_neighborhood_constructible<
    BaseNHE,
    Dependency,
    std::tuple<Args...>>
    : std::bool_constant<
          base_neighborhood_constructible_from_args<
              BaseNHE,
              Dependency,
              Args...>()>
{
};

template<class BaseNHE, class Dependency, class Tuple>
inline constexpr bool base_neighborhood_constructible_v =
    base_neighborhood_constructible<BaseNHE, Dependency, Tuple>::value;

template<class BaseNHE, class BaseArgsTuple, class... DeltaSpecs>
class neighborhood_recipe
{
public:
    using base_type = BaseNHE;
    using service_type = neighborhood_service_t<BaseNHE, DeltaSpecs...>;
    using delta_component_types = std::tuple<typename DeltaSpecs::component_type...>;

    static_assert(
        unique_types_v<typename DeltaSpecs::component_type...>,
        "a neighborhood recipe may attach at most one delta evaluator "
        "to each component type; the conflicting component type is shown in "
        "the template instantiation context");

    explicit neighborhood_recipe(BaseArgsTuple base_args)
        requires (sizeof...(DeltaSpecs) == 0)
        : base_args_{std::move(base_args)}
    {
    }

    neighborhood_recipe(
        BaseArgsTuple base_args,
        std::tuple<DeltaSpecs...> delta_specs)
        : base_args_{std::move(base_args)},
          delta_specs_{std::move(delta_specs)}
    {
    }

    template<class Component, class DeltaEvaluator, class... Args>
    [[nodiscard]]
    auto with_delta(Args&&... args) const &
    {
        static_assert(
            !type_in_pack_v<Component, typename DeltaSpecs::component_type...>,
            "a neighborhood recipe may attach at most one delta evaluator "
            "to each component type; the conflicting component type is shown in "
            "the template instantiation context");

        using spec_type = delta_spec<
            Component,
            DeltaEvaluator,
            std::decay_t<Args>...>;
        using result_type = neighborhood_recipe<
            BaseNHE,
            BaseArgsTuple,
            DeltaSpecs...,
            spec_type>;

        return result_type{
            base_args_,
            std::tuple_cat(
                delta_specs_,
                std::tuple{spec_type{std::forward<Args>(args)...}}),
        };
    }

    template<class Component, class DeltaEvaluator, class... Args>
    [[nodiscard]]
    auto with_delta(Args&&... args) &&
    {
        static_assert(
            !type_in_pack_v<Component, typename DeltaSpecs::component_type...>,
            "a neighborhood recipe may attach at most one delta evaluator "
            "to each component type; the conflicting component type is shown in "
            "the template instantiation context");

        using spec_type = delta_spec<
            Component,
            DeltaEvaluator,
            std::decay_t<Args>...>;
        using result_type = neighborhood_recipe<
            BaseNHE,
            BaseArgsTuple,
            DeltaSpecs...,
            spec_type>;

        return result_type{
            std::move(base_args_),
            std::tuple_cat(
                std::move(delta_specs_),
                std::tuple{spec_type{std::forward<Args>(args)...}}),
        };
    }

    template<class Component>
    [[nodiscard]]
    auto with_delta() const &
    {
        static_assert(
            !type_in_pack_v<Component, typename DeltaSpecs::component_type...>,
            "a neighborhood recipe may attach at most one delta evaluator "
            "to each component type; the conflicting component type is shown in "
            "the template instantiation context");

        using spec_type = colocated_delta_spec<Component>;
        using result_type = neighborhood_recipe<
            BaseNHE,
            BaseArgsTuple,
            DeltaSpecs...,
            spec_type>;

        return result_type{
            base_args_,
            std::tuple_cat(delta_specs_, std::tuple{spec_type{}}),
        };
    }

    template<class Component>
    [[nodiscard]]
    auto with_delta() &&
    {
        static_assert(
            !type_in_pack_v<Component, typename DeltaSpecs::component_type...>,
            "a neighborhood recipe may attach at most one delta evaluator "
            "to each component type; the conflicting component type is shown in "
            "the template instantiation context");

        using spec_type = colocated_delta_spec<Component>;
        using result_type = neighborhood_recipe<
            BaseNHE,
            BaseArgsTuple,
            DeltaSpecs...,
            spec_type>;

        return result_type{
            std::move(base_args_),
            std::tuple_cat(std::move(delta_specs_), std::tuple{spec_type{}}),
        };
    }

    template<class Dependency>
    static constexpr bool constructible_from =
        base_neighborhood_constructible_v<BaseNHE, Dependency, BaseArgsTuple>;

    template<class Dependency>
    [[nodiscard]]
    auto construct(Dependency& dependency) const -> service_type
    {
        auto base = std::apply(
            [&](const auto&... args) {
                if constexpr (requires { dependency.base(); })
                {
                    if constexpr (std::constructible_from<
                                      BaseNHE,
                                      decltype(dependency.base()),
                                      const decltype(args)&...>)
                    {
                        return BaseNHE{dependency.base(), args...};
                    }
                    else
                    {
                        if constexpr (sizeof...(args) == 0)
                        {
                            if constexpr (requires { typename BaseNHE::solution_manager_type; })
                            {
                                if constexpr (std::same_as<
                                    std::remove_cvref_t<decltype(dependency.base())>,
                                    typename BaseNHE::solution_manager_type>)
                                {
                                    static_assert(
                                        std::constructible_from<
                                            BaseNHE,
                                            decltype(dependency.base())>,
                                        "a NeighborhoodExplorer derived from neighborhood_explorer_base "
                                        "must inherit the base constructors; did you forget "
                                        "`using neighborhood_explorer_base::neighborhood_explorer_base;`?");
                                }
                                else
                                {
                                    static_assert(
                                        std::constructible_from<
                                            BaseNHE,
                                            Dependency&,
                                            const decltype(args)&...>,
                                        "a NeighborhoodExplorer must be constructible from "
                                        "the configured SolutionManager (or its base) followed "
                                        "by its recipe arguments");
                                }
                            }
                            else
                            {
                                static_assert(
                                    std::constructible_from<
                                        BaseNHE,
                                        Dependency&,
                                        const decltype(args)&...>,
                                    "a NeighborhoodExplorer must be constructible from "
                                    "the configured SolutionManager (or its base) followed "
                                    "by its recipe arguments");
                            }
                        }
                        else
                        {
                            static_assert(
                                std::constructible_from<
                                    BaseNHE,
                                    Dependency&,
                                    const decltype(args)&...>,
                                "a NeighborhoodExplorer must be constructible from "
                                "the configured SolutionManager (or its base) followed "
                                "by its recipe arguments");
                        }
                        return BaseNHE{dependency, args...};
                    }
                }
                else
                {
                    if constexpr (sizeof...(args) == 0)
                    {
                        if constexpr (requires { typename BaseNHE::solution_manager_type; })
                        {
                            if constexpr (std::same_as<
                                std::remove_cvref_t<Dependency>,
                                typename BaseNHE::solution_manager_type>)
                            {
                                static_assert(
                                    std::constructible_from<BaseNHE, Dependency&>,
                                    "a NeighborhoodExplorer derived from neighborhood_explorer_base "
                                    "must inherit the base constructors; did you forget "
                                    "`using neighborhood_explorer_base::neighborhood_explorer_base;`?");
                            }
                            else
                            {
                                static_assert(
                                    std::constructible_from<
                                        BaseNHE,
                                        Dependency&,
                                        const decltype(args)&...>,
                                    "a NeighborhoodExplorer must be constructible from "
                                    "the configured SolutionManager followed by its recipe "
                                    "arguments");
                            }
                        }
                        else
                        {
                            static_assert(
                                std::constructible_from<
                                    BaseNHE,
                                    Dependency&,
                                    const decltype(args)&...>,
                                "a NeighborhoodExplorer must be constructible from "
                                "the configured SolutionManager followed by its recipe "
                                "arguments");
                        }
                    }
                    else
                    {
                        static_assert(
                            std::constructible_from<
                                BaseNHE,
                                Dependency&,
                                const decltype(args)&...>,
                            "a NeighborhoodExplorer must be constructible from "
                            "the configured SolutionManager followed by its recipe "
                            "arguments");
                    }
                    return BaseNHE{dependency, args...};
                }
            },
            base_args_);

        if constexpr (sizeof...(DeltaSpecs) == 0)
        {
            return base;
        }
        else
        {
            return std::apply(
                [&](const auto&... specs) {
                    return service_type{
                        std::move(base),
                        specs.construct(dependency)...,
                    };
                },
                delta_specs_);
        }
    }

private:
    BaseArgsTuple base_args_;
    std::tuple<DeltaSpecs...> delta_specs_;
};

template<class... ComponentSpecs>
inline constexpr bool unique_component_specs_v = unique_types_v<
    typename ComponentSpecs::component_type...>;

template<class... DeltaSpecs>
inline constexpr bool unique_delta_component_specs_v = unique_types_v<
    typename DeltaSpecs::component_type...>;

template<
    class BaseSM,
    class BaseArgsTuple,
    class... ComponentSpecs,
    class Component,
    class... Args>
[[nodiscard]]
auto operator|(
    solution_manager_recipe<
        BaseSM,
        BaseArgsTuple,
        ComponentSpecs...> recipe,
    component_spec<Component, Args...> spec)
{
    return std::apply(
        [&]<class... StoredArgs>(StoredArgs&&... args) {
            return std::move(recipe).template with_component<Component>(
                std::forward<StoredArgs>(args)...);
        },
        std::move(spec).args());
}

template<
    class BaseSM,
    class BaseArgsTuple,
    class... ComponentSpecs,
    class Aggregator>
[[nodiscard]]
auto operator|(
    solution_manager_recipe<
        BaseSM,
        BaseArgsTuple,
        ComponentSpecs...> recipe,
    aggregator_spec<Aggregator> spec)
{
    return std::move(recipe).with_aggregator(std::move(spec).get());
}

template<
    class BaseNHE,
    class BaseArgsTuple,
    class... DeltaSpecs,
    class Component,
    class DeltaEvaluator,
    class... Args>
[[nodiscard]]
auto operator|(
    neighborhood_recipe<
        BaseNHE,
        BaseArgsTuple,
        DeltaSpecs...> recipe,
    delta_spec<Component, DeltaEvaluator, Args...> spec)
{
    return std::apply(
        [&]<class... StoredArgs>(StoredArgs&&... args) {
            return std::move(recipe)
                .template with_delta<Component, DeltaEvaluator>(
                    std::forward<StoredArgs>(args)...);
        },
        std::move(spec).args());
}

template<
    class BaseNHE,
    class BaseArgsTuple,
    class... DeltaSpecs,
    class Component>
[[nodiscard]]
auto operator|(
    neighborhood_recipe<
        BaseNHE,
        BaseArgsTuple,
        DeltaSpecs...> recipe,
    colocated_delta_spec<Component>)
{
    return std::move(recipe).template with_delta<Component>();
}

} // namespace easylocal::detail
