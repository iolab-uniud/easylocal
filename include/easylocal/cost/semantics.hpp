#pragma once

/// \file
/// Semantic cost relations used by search algorithms.
///
/// They are resolved through the SolutionManager: the function of a cost::apply
/// at the root of the cost expression may customize them (better(...),
/// equivalent(...), better_or_equivalent(...)); otherwise the cost type's own
/// operators <, ==, <= are the exact default.

#include <concepts>

namespace easylocal::cost
{

namespace detail
{

// Cost semantics belong to the cost layer, not to the problem's
// SolutionManager. The root of the cost expression may customize the semantic
// relations of its cost; otherwise the intrinsic cost operators are the
// exact/default fallback.
//
// Keep better, equivalent, and better_or_equivalent as independent queries.
// In particular, do not implement better_or_equivalent as better || equivalent:
// a cost model may answer the <= relation in a single pass over only the
// components needed to decide it.

template<class SM>
concept custom_better =
    requires(
        const SM& solution_manager,
        const typename SM::cost_type& candidate,
        const typename SM::cost_type& reference)
    {
        {
            solution_manager.cost_expression().better(candidate, reference)
        } -> std::convertible_to<bool>;
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
concept custom_equivalent =
    requires(
        const SM& solution_manager,
        const typename SM::cost_type& lhs,
        const typename SM::cost_type& rhs)
    {
        {
            solution_manager.cost_expression().equivalent(lhs, rhs)
        } -> std::convertible_to<bool>;
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
concept custom_better_or_equivalent =
    requires(
        const SM& solution_manager,
        const typename SM::cost_type& candidate,
        const typename SM::cost_type& reference)
    {
        {
            solution_manager.cost_expression().better_or_equivalent(
                candidate,
                reference)
        } -> std::convertible_to<bool>;
    };

template<class SM>
concept intrinsic_better_or_equivalent =
    requires(
        const typename SM::cost_type& candidate,
        const typename SM::cost_type& reference)
    {
        { candidate <= reference } -> std::convertible_to<bool>;
    };

} // namespace detail

/// A SolutionManager whose costs have a `better` relation: defined by the
/// function of a root `cost::apply`, or the cost type's `<`.
template<class SM>
concept has_better = detail::custom_better<SM> || detail::intrinsic_better<SM>;

/// Whether `candidate` is better than `reference`.
///
/// It asks the function of a root `cost::apply` when it defines `better`, and
/// otherwise compares with `<`.
template<class SM>
    requires has_better<SM>
[[nodiscard]]
constexpr bool better(
    const SM& solution_manager,
    const typename SM::cost_type& candidate,
    const typename SM::cost_type& reference)
{
    if constexpr (detail::custom_better<SM>)
    {
        return static_cast<bool>(
            solution_manager.cost_expression().better(candidate, reference));
    }
    else
    {
        return static_cast<bool>(candidate < reference);
    }
}

/// A SolutionManager whose costs have an `equivalent` relation: defined by the
/// function of a root `cost::apply`, or the cost type's `==`.
template<class SM>
concept has_equivalent =
    detail::custom_equivalent<SM> || detail::intrinsic_equivalent<SM>;

/// Whether `lhs` and `rhs` are equivalent costs.
///
/// It asks the function of a root `cost::apply` when it defines `equivalent`,
/// and otherwise compares with `==`.
template<class SM>
    requires has_equivalent<SM>
[[nodiscard]]
constexpr bool equivalent(
    const SM& solution_manager,
    const typename SM::cost_type& lhs,
    const typename SM::cost_type& rhs)
{
    if constexpr (detail::custom_equivalent<SM>)
    {
        return static_cast<bool>(
            solution_manager.cost_expression().equivalent(lhs, rhs));
    }
    else
    {
        return static_cast<bool>(lhs == rhs);
    }
}

/// A SolutionManager whose costs have a `better_or_equivalent` relation:
/// defined by the function of a root `cost::apply`, or the cost type's `<=`.
template<class SM>
concept has_better_or_equivalent =
    detail::custom_better_or_equivalent<SM> ||
    detail::intrinsic_better_or_equivalent<SM>;

/// Whether `candidate` is better than or equivalent to `reference`.
///
/// It asks the function of a root `cost::apply` when it defines
/// `better_or_equivalent`, and otherwise compares with `<=`.
template<class SM>
    requires has_better_or_equivalent<SM>
[[nodiscard]]
constexpr bool better_or_equivalent(
    const SM& solution_manager,
    const typename SM::cost_type& candidate,
    const typename SM::cost_type& reference)
{
    if constexpr (detail::custom_better_or_equivalent<SM>)
    {
        return static_cast<bool>(
            solution_manager.cost_expression().better_or_equivalent(
                candidate,
                reference));
    }
    else
    {
        return static_cast<bool>(candidate <= reference);
    }
}

} // namespace easylocal::cost
