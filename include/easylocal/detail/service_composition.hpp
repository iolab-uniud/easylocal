#pragma once

#include <easylocal/detail/solution_manager_concepts.hpp>

#include <concepts>
#include <cstddef>
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
        static_assert(
            std::constructible_from<Component, const Instance&, const StoredArgs&...>,
            "a cost component must be constructible from the bound Instance "
            "followed by its recipe arguments");

        return std::apply(
            [&](const auto&... args) {
                return Component{instance, args...};
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

    template<class Instance>
    [[nodiscard]]
    auto construct(const Instance& instance) const -> binding_type
    {
        static_assert(
            std::constructible_from<
                DeltaEvaluator,
                const Instance&,
                const StoredArgs&...>,
            "a delta evaluator must be constructible from the bound Instance "
            "followed by its recipe arguments");

        return std::apply(
            [&](const auto&... args) {
                return binding_type{
                    DeltaEvaluator{instance, args...},
                };
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

template<class BaseSM, class ValuesTuple>
struct aggregated_cost_type;

template<class BaseSM, class... Values>
struct aggregated_cost_type<BaseSM, std::tuple<Values...>>
{
    using type = decltype(
        std::declval<const BaseSM&>().aggregate(
            std::declval<const Values&>()...));
};

template<class BaseSM, class ValuesTuple>
using aggregated_cost_type_t =
    typename aggregated_cost_type<BaseSM, ValuesTuple>::type;

template<class BaseSM, class... ComponentSpecs>
class configured_solution_manager
{
public:
    using base_type = BaseSM;
    using instance_type = typename BaseSM::instance_type;
    using solution_type = typename BaseSM::solution_type;
    using component_types = std::tuple<typename ComponentSpecs::component_type...>;
    using component_values_type = std::tuple<
        typename ComponentSpecs::component_type::value_type...>;

    static_assert(
        unique_types_v<typename ComponentSpecs::component_type...>,
        "a SolutionManager recipe may contain each component type at most once; "
        "the conflicting component type is shown in the template instantiation "
        "context");

    using cost_type = aggregated_cost_type_t<BaseSM, component_values_type>;

    configured_solution_manager(
        BaseSM base,
        const ComponentSpecs&... component_specs)
        : base_{std::move(base)},
          components_{component_specs.construct(base_.instance())...}
    {
        static_assert(sizeof...(ComponentSpecs) > 0,
            "a configured SolutionManager needs at least one cost component");
    }

    [[nodiscard]]
    auto base() noexcept -> BaseSM&
    {
        return base_;
    }

    [[nodiscard]]
    auto base() const noexcept -> const BaseSM&
    {
        return base_;
    }

    [[nodiscard]]
    auto instance() const noexcept -> const instance_type&
    {
        return base_.instance();
    }

    [[nodiscard]]
    auto is_valid(const solution_type& solution) const noexcept(noexcept(
        std::declval<const BaseSM&>().is_valid(solution))) -> bool
    {
        return base_.is_valid(solution);
    }

    // Preserve optional solution-construction capabilities of the base
    // SolutionManager. These remain optional: configuring cost components must
    // neither add nor remove the ability to construct a solution.
    [[nodiscard]]
    auto initial_solution() const noexcept(noexcept(
        std::declval<const BaseSM&>().initial_solution())) -> solution_type
        requires has_initial_solution<BaseSM>
    {
        return base_.initial_solution();
    }

    template<class RNG>
    [[nodiscard]]
    auto random_solution(RNG& rng) const noexcept(noexcept(
        std::declval<const BaseSM&>().random_solution(rng))) -> solution_type
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
                return component_values_type{
                    component.evaluate(solution)...,
                };
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

    [[nodiscard]]
    auto aggregate(const component_values_type& values) const -> cost_type
    {
        return std::apply(
            [&](const auto&... value) -> cost_type {
                return base_.aggregate(value...);
            },
            values);
    }

    [[nodiscard]]
    auto evaluate(const solution_type& solution) const -> cost_type
    {
        return aggregate(evaluate_components(solution));
    }

    // Preserve optional problem-specific cost semantics across the
    // configured SolutionManager wrapper. If the base manager does not provide
    // one of these queries, runner_context can still fall back to the
    // corresponding intrinsic operator of cost_type when available.
    [[nodiscard]]
    constexpr auto better(
        const cost_type& candidate,
        const cost_type& reference) const -> bool
        requires requires(const BaseSM& base) {
            { base.better(candidate, reference) } -> std::convertible_to<bool>;
        }
    {
        return static_cast<bool>(base_.better(candidate, reference));
    }

    [[nodiscard]]
    constexpr auto equivalent(
        const cost_type& lhs,
        const cost_type& rhs) const -> bool
        requires requires(const BaseSM& base) {
            { base.equivalent(lhs, rhs) } -> std::convertible_to<bool>;
        }
    {
        return static_cast<bool>(base_.equivalent(lhs, rhs));
    }

    [[nodiscard]]
    constexpr auto better_or_equivalent(
        const cost_type& candidate,
        const cost_type& reference) const -> bool
        requires requires(const BaseSM& base) {
            {
                base.better_or_equivalent(candidate, reference)
            } -> std::convertible_to<bool>;
        }
    {
        return static_cast<bool>(
            base_.better_or_equivalent(candidate, reference));
    }

private:
    BaseSM base_;
    std::tuple<typename ComponentSpecs::component_type...> components_;
};

template<class BaseSM, class... ComponentSpecs>
using solution_manager_service_t = std::conditional_t<
    sizeof...(ComponentSpecs) == 0,
    BaseSM,
    configured_solution_manager<BaseSM, ComponentSpecs...>>;

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
    using base_type = BaseSM;
    using service_type = solution_manager_service_t<BaseSM, ComponentSpecs...>;
    using component_types = std::tuple<typename ComponentSpecs::component_type...>;

    static_assert(
        unique_types_v<typename ComponentSpecs::component_type...>,
        "a SolutionManager recipe may contain each component type at most once; "
        "the conflicting component type is shown in the template instantiation "
        "context");

    explicit solution_manager_recipe(BaseArgsTuple base_args)
        requires (sizeof...(ComponentSpecs) == 0)
        : base_args_{std::move(base_args)}
    {
    }

    solution_manager_recipe(
        BaseArgsTuple base_args,
        std::tuple<ComponentSpecs...> component_specs)
        : base_args_{std::move(base_args)},
          component_specs_{std::move(component_specs)}
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

    template<class Dependency>
    static constexpr bool constructible_from =
        std::same_as<
            std::remove_cvref_t<Dependency>,
            typename BaseSM::instance_type> &&
        base_solution_manager_constructible_v<
            BaseSM,
            typename BaseSM::instance_type,
            BaseArgsTuple>;

    [[nodiscard]]
    auto construct(const typename BaseSM::instance_type& instance) const
        -> service_type
    {
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
            return std::apply(
                [&](const auto&... specs) {
                    return service_type{std::move(base), specs...};
                },
                component_specs_);
        }
    }

private:
    BaseArgsTuple base_args_;
    std::tuple<ComponentSpecs...> component_specs_;
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
                        static_assert(
                            std::constructible_from<
                                BaseNHE,
                                Dependency&,
                                const decltype(args)&...>,
                            "a NeighborhoodExplorer must be constructible from "
                            "the configured SolutionManager (or its base) followed "
                            "by its recipe arguments");
                        return BaseNHE{dependency, args...};
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
                        specs.construct(dependency.instance())...,
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

} // namespace easylocal::detail
