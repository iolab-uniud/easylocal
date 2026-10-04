#pragma once

/// \file
/// Semantic cost relations used by search algorithms. They are resolved through
/// the SolutionManager: the function of a cost::apply at the root of the cost
/// expression may customize them (better(...), equivalent(...),
/// better_or_equivalent(...)); otherwise the cost type's own operators <, ==,
/// <= are the exact default.

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
// a future lazy cost model must be free to answer the <= semantic relation in a
// single pass over only the components needed to decide it.

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

template<class SM>
concept has_better = detail::custom_better<SM> || detail::intrinsic_better<SM>;

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

template<class SM>
concept has_equivalent =
    detail::custom_equivalent<SM> || detail::intrinsic_equivalent<SM>;

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

template<class SM>
concept has_better_or_equivalent =
    detail::custom_better_or_equivalent<SM> ||
    detail::intrinsic_better_or_equivalent<SM>;

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
