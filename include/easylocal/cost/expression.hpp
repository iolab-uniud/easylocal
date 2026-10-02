#pragma once

#include <easylocal/cost/concepts.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

// Cost expressions: the structure of a cost, written over the cost components
// of a SolutionManager recipe.
//
//   solution_manager<SM>()
//       | cost::hard_soft(
//             cost::sum(component<A>(), component<B>() * 10),
//             component<C>())
//
// A leaf is a cost component (component<C>(args...)); a node combines the
// costs of its children:
// - cost::sum(terms...)          Σ wᵢ · costᵢ over arithmetic costs, weights
//                                configurable as `weights` (1 unless
//                                cost::weighted(child, w), or its shorthand
//                                child * w, gives one);
// - cost::in_order(children...)  cost::lexicographic of the children's costs;
// - cost::hard_soft(hard, soft)  cost::hierarchical; TwoStage evaluates only
//                                the components under `hard` in its first stage;
// - cost::apply(f, children...)  f(costs...), any user function; f may also
//                                define better / equivalent /
//                                better_or_equivalent (at the root) and
//                                configuration().
//
// The expression types below only record the structure; they are given meaning
// by the SolutionManager recipe, which knows the components' value types.
namespace easylocal::cost
{

template<class Child, class Weight>
struct weighted_term
{
    Child child;
    Weight weight;
};

template<class... Terms>
struct sum_expression
{
    std::tuple<Terms...> terms;
};

template<class... Children>
struct in_order_expression
{
    std::tuple<Children...> children;
};

template<class Hard, class Soft>
struct hard_soft_expression
{
    Hard hard;
    Soft soft;
};

template<class Function, class... Children>
struct apply_expression
{
    Function function;
    std::tuple<Children...> children;
};

template<class Child, arithmetic Weight>
[[nodiscard]]
constexpr auto weighted(Child child, Weight weight)
    -> weighted_term<Child, Weight>
{
    return {std::move(child), weight};
}

template<class... Terms>
    requires (sizeof...(Terms) > 0)
[[nodiscard]]
constexpr auto sum(Terms... terms) -> sum_expression<Terms...>
{
    return {std::tuple<Terms...>{std::move(terms)...}};
}

template<class... Children>
    requires (sizeof...(Children) > 0)
[[nodiscard]]
constexpr auto in_order(Children... children)
    -> in_order_expression<Children...>
{
    return {std::tuple<Children...>{std::move(children)...}};
}

template<class Hard, class Soft>
[[nodiscard]]
constexpr auto hard_soft(Hard hard, Soft soft)
    -> hard_soft_expression<Hard, Soft>
{
    return {std::move(hard), std::move(soft)};
}

template<class Function, class... Children>
    requires (sizeof...(Children) > 0)
[[nodiscard]]
constexpr auto apply(Function function, Children... children)
    -> apply_expression<Function, Children...>
{
    return {
        std::move(function),
        std::tuple<Children...>{std::move(children)...},
    };
}

template<class T>
struct is_expression : std::false_type
{
};

template<class... Terms>
struct is_expression<sum_expression<Terms...>> : std::true_type
{
};

template<class... Children>
struct is_expression<in_order_expression<Children...>> : std::true_type
{
};

template<class Hard, class Soft>
struct is_expression<hard_soft_expression<Hard, Soft>> : std::true_type
{
};

template<class Function, class... Children>
struct is_expression<apply_expression<Function, Children...>> : std::true_type
{
};

template<class T>
inline constexpr bool is_expression_v =
    is_expression<std::remove_cvref_t<T>>::value;

// child * w and w * child are cost::weighted(child, w).
template<class Child, arithmetic Weight>
    requires is_expression_v<Child>
[[nodiscard]]
constexpr auto operator*(Child child, Weight weight)
    -> weighted_term<Child, Weight>
{
    return weighted(std::move(child), weight);
}

template<arithmetic Weight, class Child>
    requires is_expression_v<Child>
[[nodiscard]]
constexpr auto operator*(Weight weight, Child child)
    -> weighted_term<Child, Weight>
{
    return weighted(std::move(child), weight);
}

} // namespace easylocal::cost
