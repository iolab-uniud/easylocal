#pragma once

/// \file
/// Composition vocabulary for problem-side components: SolutionManager recipes
/// (solution_manager<SM>() | component<C>(), or a cost expression over several
/// components, see <easylocal/cost/expression.hpp>) and neighborhood recipes
/// (neighborhood<NHE>() | delta<C, D>()).
///
/// Recipes are constructed lazily from the bound Input by runners and apps.

#include <easylocal/config/detail/parameterized.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/helpers/detail/neighborhood_recipe.hpp>
#include <easylocal/helpers/detail/solution_manager_recipe.hpp>
#include <easylocal/helpers/solution_manager.hpp>

#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal
{

namespace detail
{

template<class T>
struct is_solution_manager_spec : std::false_type
{
};

template<class BaseSM, class BaseArgsTuple>
struct is_solution_manager_spec<solution_manager_recipe<BaseSM, BaseArgsTuple>>
    : std::true_type
{
};

template<class BaseSM, class BaseArgsTuple, class Expression>
struct is_solution_manager_spec<
    solution_manager_with_cost_recipe<BaseSM, BaseArgsTuple, Expression>>
    : std::true_type
{
};

template<class T>
inline constexpr bool is_solution_manager_spec_v =
    is_solution_manager_spec<T>::value;

template<class T>
struct is_costless_solution_manager_recipe : std::false_type
{
};

template<class BaseSM, class BaseArgsTuple>
struct is_costless_solution_manager_recipe<
    solution_manager_recipe<BaseSM, BaseArgsTuple>> : std::true_type
{
};

// Checks a SolutionManager recipe before a runner or an app accepts it, so that
// a malformed recipe fails with a direct diagnostic.
template<class Spec>
consteval bool validate_solution_manager_spec()
{
    static_assert(
        !is_costless_solution_manager_recipe<Spec>::value,
        "a SolutionManager recipe needs a cost: the cost is always computed by "
        "cost components, add `| component<C>()` or a cost expression such as "
        "`| cost::sum(component<A>(), component<B>())`");
    return true;
}

template<class T>
struct is_neighborhood_spec : std::false_type
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

/// The recipe of a SolutionManager constructed from the Input and `args`.
///
/// It needs a cost: `| component<C>()` or a cost expression, such as
/// `| cost::sum(component<A>(), component<B>())`. For a SolutionManager whose
/// parameters_type is a parameter block, a first argument of that type gives
/// its parameters, which otherwise are the defaults, and follows the Input in
/// its construction arguments; they are configurable under
/// `solution_manager.*` in a runner or an app.
template<class SM, class... Args>
[[nodiscard]]
auto solution_manager(Args&&... args)
{
    if constexpr (config::detail::parameterized<SM> && sizeof...(Args) > 0)
    {
        return [](auto&& first, auto&&... rest) {
            if constexpr (std::same_as<
                              std::remove_cvref_t<decltype(first)>,
                              typename SM::parameters_type>)
            {
                return detail::solution_manager_recipe<
                    SM,
                    std::tuple<std::decay_t<decltype(rest)>...>>{
                    std::tuple<std::decay_t<decltype(rest)>...>{
                        std::forward<decltype(rest)>(rest)...},
                    std::forward<decltype(first)>(first)};
            }
            else
            {
                return detail::solution_manager_recipe<
                    SM,
                    std::tuple<
                        std::decay_t<decltype(first)>,
                        std::decay_t<decltype(rest)>...>>{
                    std::tuple<
                        std::decay_t<decltype(first)>,
                        std::decay_t<decltype(rest)>...>{
                        std::forward<decltype(first)>(first),
                        std::forward<decltype(rest)>(rest)...}};
            }
        }(std::forward<Args>(args)...);
    }
    else
    {
        return detail::solution_manager_recipe<SM, std::tuple<std::decay_t<Args>...>>{
            std::tuple<std::decay_t<Args>...>{std::forward<Args>(args)...}};
    }
}

/// A cost component, a leaf of a cost expression, constructed from the Input
/// and `args` (or from `args` alone).
///
/// For a component whose parameters_type is a parameter block, a first
/// argument of that type gives its parameters, which otherwise are the
/// defaults, and lead its construction arguments; they are configurable under
/// its static name(), `cost.<name>.*` in a runner or an app.
template<class Component, class... Args>
[[nodiscard]]
auto component(Args&&... args)
{
    if constexpr (config::detail::parameterized<Component> && sizeof...(Args) > 0)
    {
        return [](auto&& first, auto&&... rest) {
            if constexpr (std::same_as<
                              std::remove_cvref_t<decltype(first)>,
                              typename Component::parameters_type>)
            {
                return detail::component_spec<Component, std::decay_t<decltype(rest)>...>{
                    std::forward<decltype(first)>(first),
                    std::forward<decltype(rest)>(rest)...};
            }
            else
            {
                return detail::component_spec<
                    Component,
                    std::decay_t<decltype(first)>,
                    std::decay_t<decltype(rest)>...>{
                    std::forward<decltype(first)>(first),
                    std::forward<decltype(rest)>(rest)...};
            }
        }(std::forward<Args>(args)...);
    }
    else
    {
        return detail::component_spec<Component, std::decay_t<Args>...>{
            std::forward<Args>(args)...};
    }
}

/// The recipe of a NeighborhoodExplorer constructed from the SolutionManager
/// and args.
///
/// For an explorer whose parameters_type is a parameter block, a first
/// argument of that type gives its parameters, which otherwise are the
/// defaults; they are configurable under `neighborhood.*` (or
/// `runners.<name>.neighborhood.*` for a runner's own neighborhood).
template<class NHE, class... Args>
[[nodiscard]]
auto neighborhood(Args&&... args)
{
    if constexpr (config::detail::parameterized<NHE> && sizeof...(Args) > 0)
    {
        return [](auto&& first, auto&&... rest) {
            if constexpr (std::same_as<
                              std::remove_cvref_t<decltype(first)>,
                              typename NHE::parameters_type>)
            {
                return detail::neighborhood_recipe<
                    NHE,
                    std::tuple<std::decay_t<decltype(rest)>...>>{
                    std::tuple<std::decay_t<decltype(rest)>...>{
                        std::forward<decltype(rest)>(rest)...},
                    std::forward<decltype(first)>(first)};
            }
            else
            {
                return detail::neighborhood_recipe<
                    NHE,
                    std::tuple<
                        std::decay_t<decltype(first)>,
                        std::decay_t<decltype(rest)>...>>{
                    std::tuple<
                        std::decay_t<decltype(first)>,
                        std::decay_t<decltype(rest)>...>{
                        std::forward<decltype(first)>(first),
                        std::forward<decltype(rest)>(rest)...}};
            }
        }(std::forward<Args>(args)...);
    }
    else
    {
        return detail::neighborhood_recipe<NHE, std::tuple<std::decay_t<Args>...>>{
            std::tuple<std::decay_t<Args>...>{std::forward<Args>(args)...}};
    }
}

/// The delta cost component co-located in `Component`, its `delta_evaluate`,
/// bound to a neighborhood recipe with `neighborhood<NHE>() | delta<C>()`.
template<class Component>
[[nodiscard]]
auto delta()
{
    return detail::colocated_delta_spec<Component>{};
}

/// The delta cost component `DeltaEvaluator` of `Component`, constructed from
/// the Input and `args` (or from `args` alone), bound to a neighborhood recipe
/// with `neighborhood<NHE>() | delta<C, D>(args...)`.
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
