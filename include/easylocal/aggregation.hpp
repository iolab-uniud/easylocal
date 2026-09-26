#pragma once

#include <easylocal/cost.hpp>

#include <compare>
#include <concepts>
#include <cstddef>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::aggregation
{

namespace detail
{

struct lexicographic_tag
{
};

template<class Tag, class... Values>
class ordered_cost
{
public:
    constexpr explicit ordered_cost(Values... values)
        : values_{std::move(values)...}
    {
    }

    template<std::size_t Index>
    [[nodiscard]]
    constexpr auto get() const noexcept
        -> const std::tuple_element_t<Index, std::tuple<Values...>>&
    {
        return std::get<Index>(values_);
    }

    auto operator<=>(const ordered_cost&) const = default;

private:
    std::tuple<Values...> values_;
};

} // namespace detail

template<class... Values>
using lexicographic_cost =
    detail::ordered_cost<detail::lexicographic_tag, Values...>;

template<class HardCost, class SoftCost>
class hierarchical_cost
{
public:
    using hard_cost_type = HardCost;
    using soft_cost_type = SoftCost;

    constexpr explicit hierarchical_cost(HardCost hard, SoftCost soft)
        : hard_{std::move(hard)},
          soft_{std::move(soft)}
    {
    }

    [[nodiscard]]
    constexpr auto hard() const noexcept -> const HardCost&
    {
        return hard_;
    }

    [[nodiscard]]
    constexpr auto soft() const noexcept -> const SoftCost&
    {
        return soft_;
    }

    auto operator<=>(const hierarchical_cost&) const = default;

    [[nodiscard]]
    friend constexpr auto delta(
        const hierarchical_cost& candidate,
        const hierarchical_cost& current) -> long double
        requires requires(const HardCost& lhs, const HardCost& rhs) {
            { lhs < rhs } -> std::convertible_to<bool>;
            { lhs == rhs } -> std::convertible_to<bool>;
        } && delta_cost<SoftCost>
    {
        if (candidate.hard() < current.hard())
        {
            return -std::numeric_limits<long double>::infinity();
        }
        if (current.hard() < candidate.hard())
        {
            return std::numeric_limits<long double>::infinity();
        }
        if (candidate.hard() == current.hard())
        {
            return static_cast<long double>(
                delta(candidate.soft(), current.soft()));
        }

        // A hierarchical cost requires a total ordering of the hard branch for
        // a meaningful numeric delta. Conservatively make an unordered hard
        // transition unacceptable to delta-based algorithms.
        return std::numeric_limits<long double>::infinity();
    }

    [[nodiscard]]
    friend constexpr auto operator-(
        const hierarchical_cost& candidate,
        const hierarchical_cost& current) -> long double
        requires delta_cost<hierarchical_cost>
    {
        return delta(candidate, current);
    }

private:
    HardCost hard_;
    SoftCost soft_;
};


template<class T>
struct is_hierarchical_cost : std::false_type
{
};

template<class HardCost, class SoftCost>
struct is_hierarchical_cost<hierarchical_cost<HardCost, SoftCost>>
    : std::true_type
{
};

template<class T>
inline constexpr bool is_hierarchical_cost_v =
    is_hierarchical_cost<std::remove_cvref_t<T>>::value;

template<class T>
concept hierarchical_cost_type = is_hierarchical_cost_v<T>;

struct lexicographic
{
    template<class... Values>
    [[nodiscard]]
    constexpr auto operator()(Values&&... values) const
    {
        return lexicographic_cost<std::remove_cvref_t<Values>...>{
            std::forward<Values>(values)...,
        };
    }
};

struct hierarchical
{
    template<class HardCost, class SoftCost>
    [[nodiscard]]
    constexpr auto operator()(HardCost&& hard, SoftCost&& soft) const
    {
        return hierarchical_cost<
            std::remove_cvref_t<HardCost>,
            std::remove_cvref_t<SoftCost>>{
            std::forward<HardCost>(hard),
            std::forward<SoftCost>(soft),
        };
    }
};

template<class... Weights>
class weighted_sum
{
public:
    static_assert(
        sizeof...(Weights) > 0,
        "weighted_sum requires at least one weight");

    constexpr explicit weighted_sum(Weights... weights)
        : weights_{std::move(weights)...}
    {
    }

    template<class... Values>
        requires (sizeof...(Values) == sizeof...(Weights))
    [[nodiscard]]
    constexpr auto operator()(Values&&... values) const
    {
        return std::apply(
            [&](const auto&... weights) {
                return (... +
                    (weights * std::forward<Values>(values)));
            },
            weights_);
    }

private:
    std::tuple<Weights...> weights_;
};

template<class... Weights>
weighted_sum(Weights...)
    -> weighted_sum<std::remove_cvref_t<Weights>...>;

} // namespace easylocal::aggregation
