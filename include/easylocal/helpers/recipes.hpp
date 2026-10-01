#pragma once

#include <easylocal/config/tree.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/helpers/detail/service_composition.hpp>
#include <easylocal/helpers/solution_manager.hpp>

#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>

// Composition vocabulary for problem-side components: SolutionManager recipes
// (solution_manager<SM>() | component<C>() | aggregator(A)) and neighborhood
// recipes (neighborhood<NHE>() | delta<C, D>()). Recipes are constructed
// lazily from the bound Input by runners and apps.
namespace easylocal
{

namespace detail
{

struct solution_manager_tag
{
};

struct neighborhood_tag
{
};

template<class Tag, class Service, class... StoredArgs>
class service_spec
{
public:
    using tag_type = Tag;
    using service_type = Service;

    explicit service_spec(StoredArgs... args)
        : args_{std::move(args)...}
    {
    }

    template<class Dependency>
    static constexpr bool constructible_from =
        std::constructible_from<
            Service,
            Dependency&,
            const StoredArgs&...>;

    template<class Dependency>
        requires constructible_from<Dependency>
    [[nodiscard]]
    auto construct(Dependency& dependency) const -> Service
    {
        return std::apply(
            [&](const auto&... args) {
                return Service{dependency, args...};
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

template<class T>
struct is_solution_manager_spec : std::false_type
{
};

template<class Service, class... Args>
struct is_solution_manager_spec<
    service_spec<solution_manager_tag, Service, Args...>> : std::true_type
{
};

template<class BaseSM, class BaseArgsTuple, class... ComponentSpecs>
struct is_solution_manager_spec<
    solution_manager_recipe<BaseSM, BaseArgsTuple, ComponentSpecs...>>
    : std::true_type
{
};

template<class SMSpec, class AggregatorSpec>
struct is_solution_manager_spec<
    solution_manager_with_aggregator_recipe<SMSpec, AggregatorSpec>>
    : std::true_type
{
};

template<class T>
inline constexpr bool is_solution_manager_spec_v =
    is_solution_manager_spec<T>::value;

template<class T>
struct is_unaggregated_component_solution_manager_recipe : std::false_type
{
};

template<class BaseSM, class BaseArgsTuple, class... ComponentSpecs>
struct is_unaggregated_component_solution_manager_recipe<
    solution_manager_recipe<BaseSM, BaseArgsTuple, ComponentSpecs...>>
    : std::bool_constant<
          (sizeof...(ComponentSpecs) > 0) &&
          !solution_manager_recipe<
              BaseSM,
              BaseArgsTuple,
              ComponentSpecs...>::has_implicit_aggregator>
{
};

template<class T>
inline constexpr bool is_unaggregated_component_solution_manager_recipe_v =
    is_unaggregated_component_solution_manager_recipe<T>::value;

template<class T>
struct is_neighborhood_spec : std::false_type
{
};

template<class Service, class... Args>
struct is_neighborhood_spec<
    service_spec<neighborhood_tag, Service, Args...>> : std::true_type
{
};

template<class BaseNHE, class BaseArgsTuple, class... DeltaSpecs>
struct is_neighborhood_spec<
    neighborhood_recipe<BaseNHE, BaseArgsTuple, DeltaSpecs...>>
    : std::true_type
{
};

template<class T>
inline constexpr bool is_neighborhood_spec_v =
    is_neighborhood_spec<T>::value;

template<class SM>
concept hierarchical_solution_manager =
    requires { typename SM::cost_type; } &&
    cost::hierarchical_type<typename SM::cost_type>;

template<class SM>
class hard_cost_solution_manager_base
{
public:
    using underlying_type = SM;
    using input_type = typename SM::input_type;
    using solution_type = typename SM::solution_type;
    using full_cost_type = typename SM::cost_type;
    using cost_type = typename full_cost_type::hard_cost_type;

    explicit hard_cost_solution_manager_base(SM solution_manager)
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
class hard_cost_solution_manager;

template<class SM>
class hard_cost_solution_manager<SM, false>
    : public hard_cost_solution_manager_base<SM>
{
public:
    using hard_cost_solution_manager_base<SM>::hard_cost_solution_manager_base;
};

template<class SM>
class hard_cost_solution_manager<SM, true>
    : public hard_cost_solution_manager_base<SM>
{
public:
    using base_type = hard_cost_solution_manager_base<SM>;
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
class hard_cost_solution_manager_spec
{
public:
    using service_type = hard_cost_solution_manager<
        typename SMSpec::service_type>;

    explicit hard_cost_solution_manager_spec(SMSpec spec)
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

template<class SMSpec>
struct is_solution_manager_spec<hard_cost_solution_manager_spec<SMSpec>>
    : std::true_type
{
};

template<class Spec>
using service_t = typename Spec::service_type;

} // namespace detail

template<class SM, class... Args>
[[nodiscard]]
auto solution_manager(Args&&... args)
{
    return detail::solution_manager_recipe<
        SM,
        std::tuple<std::decay_t<Args>...>>{
        std::tuple<std::decay_t<Args>...>{std::forward<Args>(args)...}};
}

template<class SM, class... Args>
[[nodiscard]]
auto make_solution_manager(Args&&... args)
{
    return solution_manager<SM>(std::forward<Args>(args)...);
}

template<class Component, class... Args>
[[nodiscard]]
auto component(Args&&... args)
{
    return detail::component_spec<
        Component,
        std::decay_t<Args>...>{std::forward<Args>(args)...};
}

template<class Aggregator>
[[nodiscard]]
auto aggregator(Aggregator&& value)
{
    return detail::aggregator_spec<std::remove_cvref_t<Aggregator>>{
        std::forward<Aggregator>(value)};
}

template<class NHE, class... Args>
[[nodiscard]]
auto neighborhood(Args&&... args)
{
    return detail::neighborhood_recipe<
        NHE,
        std::tuple<std::decay_t<Args>...>>{
        std::tuple<std::decay_t<Args>...>{std::forward<Args>(args)...}};
}

template<class NHE, class... Args>
[[nodiscard]]
auto make_neighborhood_explorer(Args&&... args)
{
    return neighborhood<NHE>(std::forward<Args>(args)...);
}

template<class Component>
[[nodiscard]]
auto delta()
{
    return detail::colocated_delta_spec<Component>{};
}

template<class Component, class DeltaEvaluator, class... Args>
[[nodiscard]]
auto delta(Args&&... args)
{
    return detail::delta_spec<
        Component,
        DeltaEvaluator,
        std::decay_t<Args>...>{std::forward<Args>(args)...};
}

} // namespace easylocal
