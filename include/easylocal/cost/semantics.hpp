#pragma once

/// \file
/// Semantic cost relations used by search algorithms.
///
/// They are resolved through the SolutionManager: the root of the cost
/// expression may define them all with one `compare(a, b)` returning a
/// `std::partial_ordering` (the function of a root cost::apply, or
/// cost::approximately); otherwise the cost type's own operators <, ==, <= are
/// the exact default.

#include <easylocal/cost/concepts.hpp>

#include <compare>
#include <concepts>
#include <type_traits>
#include <utility>

namespace easylocal::cost
{

namespace detail
{

// Cost semantics belong to the cost layer, not to the problem's
// SolutionManager. The root of the cost expression may define the order of its
// cost with compare(a, b) -> std::partial_ordering, from which better,
// equivalent and better_or_equivalent all follow; otherwise the intrinsic cost
// operators are the exact default.
//
// Without compare, the three relations stay independent queries: in
// particular, better_or_equivalent is not better || equivalent, so a cost type
// may answer <= in a single pass.

template<class SM>
concept custom_compare = requires(
    const SM& solution_manager,
    const typename SM::cost_type& candidate,
    const typename SM::cost_type& reference) {
    {
        solution_manager.cost_expression().compare(candidate, reference)
    } -> std::convertible_to<std::partial_ordering>;
};

template<class SM>
concept intrinsic_better =
    requires(
        const typename SM::cost_type& candidate,
        const typename SM::cost_type& reference)
    {
        { candidate < reference } -> std::convertible_to<bool>;
    };

template<class SM>
concept intrinsic_equivalent =
    requires(
        const typename SM::cost_type& lhs,
        const typename SM::cost_type& rhs)
    {
        { lhs == rhs } -> std::convertible_to<bool>;
    };

template<class SM>
concept intrinsic_better_or_equivalent = requires(
    const typename SM::cost_type& candidate,
    const typename SM::cost_type& reference) {
    { candidate <= reference } -> std::convertible_to<bool>;
};

template<class SM>
[[nodiscard]]
constexpr std::partial_ordering custom_order(
    const SM& solution_manager,
    const typename SM::cost_type& candidate,
    const typename SM::cost_type& reference)
{
    return static_cast<std::partial_ordering>(
        solution_manager.cost_expression().compare(candidate, reference));
}

// The function at the root of SM's cost expression, when the root is a
// cost::apply.
template<class SM>
using root_function_t =
    std::remove_cvref_t<decltype(std::declval<const SM&>().cost_expression().function())>;

// A compare that a default-constructed Function evaluates at compile time on
// the arithmetic costs 1 and 0.
template<class Function, class Cost>
concept constant_compare = std::default_initializable<Function> && requires {
    typename std::bool_constant<(Function{}.compare(Cost{1}, Cost{0}) < 0)>;
};

// Whether the root compare of SM is known, at compile time, to order an
// arithmetic cost against the sign of cost::delta: it finds 1 better than 0,
// whose delta is positive. Delta-based acceptance (Simulated Annealing, Great
// Deluge, the aspiration levels of Tabu Search) would then walk the other way.
template<class SM>
consteval bool compare_contradicts_delta()
{
    if constexpr (requires {
                      typename SM::cost_type;
                      typename root_function_t<SM>;
                  })
    {
        using cost_type = typename SM::cost_type;
        using function_type = root_function_t<SM>;
        if constexpr (arithmetic<cost_type> && constant_compare<function_type, cost_type>)
            return function_type{}.compare(cost_type{1}, cost_type{0}) < 0;
        else
            return false;
    }
    else
        return false;
}

} // namespace detail

/// A SolutionManager whose costs have a `better` relation: defined by the
/// `compare` of the root of its cost expression, or the cost type's `<`.
template<class SM>
concept has_better = detail::custom_compare<SM> || detail::intrinsic_better<SM>;

/// Whether `candidate` is better than `reference`.
///
/// With a `compare` at the root of the cost expression, whether it orders
/// `candidate` before `reference` (`std::partial_ordering::less`); otherwise
/// `candidate < reference`.
template<class SM>
    requires has_better<SM>
[[nodiscard]]
constexpr bool better(
    const SM& solution_manager,
    const typename SM::cost_type& candidate,
    const typename SM::cost_type& reference)
{
    if constexpr (detail::custom_compare<SM>)
        return detail::custom_order(solution_manager, candidate, reference) < 0;
    else
        return static_cast<bool>(candidate < reference);
}

/// A SolutionManager whose costs have an `equivalent` relation: defined by the
/// `compare` of the root of its cost expression, or the cost type's `==`.
template<class SM>
concept has_equivalent = detail::custom_compare<SM> || detail::intrinsic_equivalent<SM>;

/// Whether `lhs` and `rhs` are equivalent costs.
///
/// With a `compare` at the root of the cost expression, whether it finds them
/// `std::partial_ordering::equivalent`; otherwise `lhs == rhs`.
template<class SM>
    requires has_equivalent<SM>
[[nodiscard]]
constexpr bool equivalent(
    const SM& solution_manager,
    const typename SM::cost_type& lhs,
    const typename SM::cost_type& rhs)
{
    if constexpr (detail::custom_compare<SM>)
        return detail::custom_order(solution_manager, lhs, rhs) == 0;
    else
        return static_cast<bool>(lhs == rhs);
}

/// A SolutionManager whose costs have a `better_or_equivalent` relation:
/// defined by the `compare` of the root of its cost expression, or the cost
/// type's `<=`.
template<class SM>
concept has_better_or_equivalent =
    detail::custom_compare<SM> || detail::intrinsic_better_or_equivalent<SM>;

/// Whether `candidate` is better than or equivalent to `reference`.
///
/// With a `compare` at the root of the cost expression, whether it finds
/// `candidate` less than or equivalent to `reference`; otherwise
/// `candidate <= reference`.
template<class SM>
    requires has_better_or_equivalent<SM>
[[nodiscard]]
constexpr bool better_or_equivalent(
    const SM& solution_manager,
    const typename SM::cost_type& candidate,
    const typename SM::cost_type& reference)
{
    if constexpr (detail::custom_compare<SM>)
        return detail::custom_order(solution_manager, candidate, reference) <= 0;
    else
        return static_cast<bool>(candidate <= reference);
}

} // namespace easylocal::cost
