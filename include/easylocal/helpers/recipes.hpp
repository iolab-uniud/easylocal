#pragma once

#include <easylocal/config/tree.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/helpers/detail/neighborhood_recipe.hpp>
#include <easylocal/helpers/detail/solution_manager_recipe.hpp>
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

template<class SMSpec>
struct is_solution_manager_spec<hard_cost_layer_spec<SMSpec>>
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
