#pragma once

/// \file
/// Multi-objective cost ordered by Pareto dominance: a cost is better than
/// another when it is no worse in every objective and better in at least one.
/// Two costs better in different objectives are unordered, so a search with a
/// pareto cost keeps the non-dominated solutions it reaches (search_run's
/// archive). A pareto cost has no numeric delta.

#include <easylocal/cost/concepts.hpp>

#include <compare>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::cost
{

template<class... Values>
class pareto
{
public:
    static constexpr std::size_t levels = sizeof...(Values);

    constexpr explicit pareto(Values... values) : values_{std::move(values)...} {}

    template<std::size_t Index>
    [[nodiscard]]
    constexpr const std::tuple_element_t<Index, std::tuple<Values...>>& get()
        const noexcept
    {
        return std::get<Index>(values_);
    }

    /// less: lhs dominates rhs; greater: rhs dominates lhs; equivalent: equal
    /// in every objective; unordered otherwise. So < is dominance and <= weak
    /// dominance.
    [[nodiscard]]
    friend constexpr std::partial_ordering operator<=>(
        const pareto& lhs,
        const pareto& rhs)
    {
        bool lhs_better = false;
        bool rhs_better = false;
        [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            (compare_objective(
                 lhs.get<Index>(),
                 rhs.get<Index>(),
                 lhs_better,
                 rhs_better),
                ...);
        }(std::index_sequence_for<Values...>{});
        if (lhs_better && rhs_better)
            return std::partial_ordering::unordered;
        if (lhs_better)
            return std::partial_ordering::less;
        if (rhs_better)
            return std::partial_ordering::greater;
        return std::partial_ordering::equivalent;
    }

    [[nodiscard]]
    friend constexpr bool operator==(const pareto&, const pareto&) = default;

private:
    template<class Value>
    static constexpr void compare_objective(
        const Value& lhs,
        const Value& rhs,
        bool& lhs_better,
        bool& rhs_better)
    {
        if (lhs < rhs)
            lhs_better = true;
        else if (rhs < lhs)
            rhs_better = true;
    }

    std::tuple<Values...> values_;
};

template<class... Values>
    requires(has_zero<Values> && ...)
struct zero_cost<pareto<Values...>>
{
    [[nodiscard]]
    static constexpr pareto<Values...> value()
    {
        return pareto<Values...>{zero<Values>()...};
    }
};

template<class T>
struct is_pareto : std::false_type
{
};

template<class... Values>
struct is_pareto<pareto<Values...>> : std::true_type
{
};

template<class T>
inline constexpr bool is_pareto_v = is_pareto<std::remove_cvref_t<T>>::value;

template<class T>
concept pareto_type = is_pareto_v<T>;

} // namespace easylocal::cost
