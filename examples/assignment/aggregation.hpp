#pragma once

#include <compare>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::mwe::aggregation
{

namespace detail
{

struct lexicographic_tag
{
};

struct hierarchical_tag
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

template<class... Levels>
using hierarchical_cost =
    detail::ordered_cost<detail::hierarchical_tag, Levels...>;

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
    template<class... Levels>
    [[nodiscard]]
    constexpr auto operator()(Levels&&... levels) const
    {
        return hierarchical_cost<std::remove_cvref_t<Levels>...>{
            std::forward<Levels>(levels)...,
        };
    }
};

template<class... Weights>
class weighted_sum
{
public:
    static_assert(sizeof...(Weights) > 0);

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

} // namespace easylocal::mwe::aggregation
