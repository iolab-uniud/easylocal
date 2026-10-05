#pragma once

/// \file
/// Cost expressions: the structure of a cost, written over the cost components
/// of a SolutionManager recipe.
///
///   solution_manager<SM>()
///       | cost::hard_soft(
///             cost::sum(component<A>(), component<B>() * 10),
///             component<C>())
///
/// A leaf is a cost component (component<C>(args...)); a node combines the
/// costs of its children:
/// - cost::sum(terms...)          Σ wᵢ · costᵢ over arithmetic costs, weights
///                                configurable as `weights` (1 unless
///                                cost::weighted(child, w), or its shorthand
///                                child * w, gives one);
/// - cost::in_order(children...)  cost::lexicographic of the children's costs;
/// - cost::objectives(children...) cost::pareto of the children's costs,
///                                compared by Pareto dominance;
/// - cost::hard_soft(hard, soft)  cost::hierarchical; a pipeline stage
///                                until_feasible() evaluates only the
///                                components under `hard`;
/// - cost::apply(f, children...)  f(costs...), any user function; f may also
///                                define compare(a, b), the order of the costs
///                                (at the root); cost::apply<F>(parameters,
///                                children...) builds a function with
///                                parameters, configurable as `<name>.*`;
/// - cost::approximately(child)   the cost of child, compared by the search
///                                within a cost::tolerance (at the root).
///
/// The expression types below only record the structure; they are given meaning
/// by the SolutionManager recipe, which knows the components' value types.

#include <easylocal/config/detail/parameterized.hpp>
#include <easylocal/cost/concepts.hpp>
#include <easylocal/cost/tolerance.hpp>
#include <easylocal/utils/detail/attributes.hpp>

#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::cost
{

/// A term of a `cost::sum`: an expression and its weight.
template<class Child, class Weight>
struct weighted_term
{
    /// The weighted expression.
    Child child;
    /// The weight of the term in the sum.
    Weight weight;
};

/// The node of `cost::sum`: the weighted sum of the costs of its terms.
template<class... Terms>
struct sum_expression
{
    /// The terms, plain expressions or weighted terms.
    std::tuple<Terms...> terms;
};

/// The node of `cost::in_order`: a `cost::lexicographic` of its children's
/// costs.
template<class... Children>
struct in_order_expression
{
    /// The children, in order of priority.
    std::tuple<Children...> children;
};

/// The node of `cost::objectives`: a `cost::pareto` of its children's costs.
template<class... Children>
struct objectives_expression
{
    /// The children, one per objective.
    std::tuple<Children...> children;
};

/// The node of `cost::hard_soft`: a `cost::hierarchical` of a hard and a soft
/// cost.
template<class Hard, class Soft>
struct hard_soft_expression
{
    /// The hard branch, compared first.
    Hard hard;
    /// The soft branch, compared when the hard costs are equivalent.
    Soft soft;
};

/// The node of `cost::apply`: a user function of its children's costs.
template<class Function, class... Children>
struct apply_expression
{
    /// The function, called with the children's costs.
    Function function;
    /// The children, whose costs are the arguments of the function.
    std::tuple<Children...> children;
    /// The parameters the function is built from, when its parameters_type is
    /// a parameter block; empty otherwise.
    EASYLOCAL_NO_UNIQUE_ADDRESS config::detail::parameters_storage_t<Function>
        parameters{};
};

/// The node of `cost::approximately`: the cost of its child, compared within a
/// tolerance.
template<class Child>
struct approximately_expression
{
    /// The expression whose costs are compared.
    Child child;
    /// The tolerance of the comparisons.
    tolerance within;
};

/// The term `child` of a `cost::sum` with weight `weight`.
///
/// A term not weighted has weight 1; `child * weight` and `weight * child` are
/// shorthands. It is allowed only directly inside a `cost::sum`.
template<class Child, arithmetic Weight>
[[nodiscard]]
constexpr weighted_term<Child, Weight> weighted(Child child, Weight weight)
{
    return {std::move(child), weight};
}

/// The weighted sum of the costs of its terms, which must be arithmetic.
///
/// A term has weight 1 unless `cost::weighted(child, w)`, or `child * w`, gives
/// one; the weights are configurable as `weights`, one per term.
template<class... Terms>
    requires(sizeof...(Terms) > 0)
[[nodiscard]]
constexpr sum_expression<Terms...> sum(Terms... terms)
{
    return {std::tuple<Terms...>{std::move(terms)...}};
}

/// A `cost::lexicographic` of the children's costs, compared in the order
/// given.
template<class... Children>
    requires(sizeof...(Children) > 0)
[[nodiscard]]
constexpr in_order_expression<Children...> in_order(Children... children)
{
    return {std::tuple<Children...>{std::move(children)...}};
}

/// A `cost::pareto` of the costs of two or more children, compared by Pareto
/// dominance.
template<class... Children>
    requires(sizeof...(Children) > 1)
[[nodiscard]]
constexpr objectives_expression<Children...> objectives(Children... children)
{
    return {std::tuple<Children...>{std::move(children)...}};
}

/// A `cost::hierarchical` of the costs of `hard` and `soft`.
///
/// At the root of the expression, a pipeline stage until_feasible() (the first
/// stage of solvers::two_stage()) evaluates only the components under `hard`.
template<class Hard, class Soft>
[[nodiscard]]
constexpr hard_soft_expression<Hard, Soft> hard_soft(Hard hard, Soft soft)
{
    return {std::move(hard), std::move(soft)};
}

/// The cost of `child`, which the search compares within `within`: costs
/// equal within it are equivalent, and a cost is better only by more than it
/// (see cost::approximate_compare).
///
/// It defines the cost semantics, so it is the root of the expression, and
/// keeps the structure below it: over a `cost::hard_soft`, a pipeline stage
/// until_feasible() still evaluates only the hard components, and compares the
/// hard costs within the tolerance. Its parameters are `tolerance.relative` and
/// `tolerance.absolute`.
template<class Child>
[[nodiscard]]
constexpr approximately_expression<Child> approximately(
    Child child,
    const tolerance within = {})
{
    return {std::move(child), within};
}

/// The value of `function` called with the children's costs.
///
/// At the root of the expression, the function may also define the order of
/// its costs, `compare(a, b)` returning a `std::partial_ordering`, from which
/// `better` (less), `equivalent` and `better_or_equivalent` follow. A function
/// with parameters is built from them: `cost::apply<F>(parameters,
/// children...)`.
template<class Function, class... Children>
    requires(sizeof...(Children) > 0)
[[nodiscard]]
constexpr apply_expression<Function, Children...> apply(
    Function function,
    Children... children)
{
    static_assert(
        !config::detail::parameterized<Function>,
        "a cost::apply function whose parameters_type is a parameter block is "
        "built from its parameters: write cost::apply<F>(parameters, children...)");
    return {
        std::move(function),
        std::tuple<Children...>{std::move(children)...},
    };
}

/// The value of a `Function` built from `parameters`, called with the
/// children's costs: `cost::apply<Excess>({.bound = 8.0}, component<C>())`.
///
/// Its parameters are configurable under its name, `cost.<name>.*` in a
/// runner or an app, and the function is built again from them when they
/// change. Requires a function whose parameters_type is a parameter block, a
/// constructor from it, and a static name().
template<class Function, class... Children>
    requires(sizeof...(Children) > 0) && config::detail::parameterized<Function>
[[nodiscard]]
constexpr apply_expression<Function, Children...> apply(
    const typename Function::parameters_type& parameters,
    Children... children)
{
    static_assert(
        std::constructible_from<Function, const typename Function::parameters_type&>,
        "a cost::apply function whose parameters_type is a parameter block is "
        "built from it: give it a constructor from `const parameters_type&`");
    return {
        Function{parameters},
        std::tuple<Children...>{std::move(children)...},
        parameters,
    };
}

/// Whether `T` is a node of a cost expression (`sum`, `in_order`, `objectives`,
/// `hard_soft`, `apply` or `approximately`).
template<class T>
struct is_expression : std::false_type
{
};

/// A `sum` node is one.
template<class... Terms>
struct is_expression<sum_expression<Terms...>> : std::true_type
{
};

/// An `in_order` node is one.
template<class... Children>
struct is_expression<in_order_expression<Children...>> : std::true_type
{
};

/// An `objectives` node is one.
template<class... Children>
struct is_expression<objectives_expression<Children...>> : std::true_type
{
};

/// A `hard_soft` node is one.
template<class Hard, class Soft>
struct is_expression<hard_soft_expression<Hard, Soft>> : std::true_type
{
};

/// An `apply` node is one.
template<class Function, class... Children>
struct is_expression<apply_expression<Function, Children...>> : std::true_type
{
};

/// An `approximately` node is one.
template<class Child>
struct is_expression<approximately_expression<Child>> : std::true_type
{
};

/// Whether `T`, without cv and reference qualifiers, is a node of a cost
/// expression.
template<class T>
inline constexpr bool is_expression_v =
    is_expression<std::remove_cvref_t<T>>::value;

/// `child * weight` is `cost::weighted(child, weight)`.
template<class Child, arithmetic Weight>
    requires is_expression_v<Child>
[[nodiscard]]
constexpr weighted_term<Child, Weight> operator*(Child child, Weight weight)
{
    return weighted(std::move(child), weight);
}

/// `weight * child` is `cost::weighted(child, weight)`.
template<arithmetic Weight, class Child>
    requires is_expression_v<Child>
[[nodiscard]]
constexpr weighted_term<Child, Weight> operator*(Weight weight, Child child)
{
    return weighted(std::move(child), weight);
}

} // namespace easylocal::cost
