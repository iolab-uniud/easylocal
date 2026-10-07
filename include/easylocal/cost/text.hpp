#pragma once

/// \file
/// Costs written as text, for targets given on the command line, in files or
/// in the TextUI, and written back in the same form (to_text).
///
/// A number for an arithmetic cost, `[hard, soft]` for a cost::hierarchical,
/// `[v1, v2, ...]` for a cost::lexicographic or a cost::pareto, nested as the
/// types are (for example `[0, [3, 1.5]]`).

#include <easylocal/cost/concepts.hpp>
#include <easylocal/cost/hierarchical.hpp>
#include <easylocal/cost/lexicographic.hpp>
#include <easylocal/cost/pareto.hpp>
#include <easylocal/utils/detail/number_text.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::cost
{

namespace detail
{

// The elements of "[a, b, ...]", split at the commas outside nested brackets.
[[nodiscard]]
inline std::vector<std::string_view> bracketed_elements(std::string_view text)
{
    text = easylocal::detail::trim_space(text);
    if (text.size() < 2 || text.front() != '[' || text.back() != ']')
        throw std::invalid_argument{"expected [...], found '" + std::string{text} + "'"};
    // "[]" has one empty element, as "[a]" has one.
    auto elements = easylocal::detail::split_list(text.substr(1, text.size() - 2));
    if (elements.empty())
        elements.emplace_back();
    return elements;
}

[[nodiscard]]
inline std::vector<std::string_view> elements_of(
    std::string_view text,
    const std::size_t count,
    const std::string_view what)
{
    auto elements = bracketed_elements(text);
    if (elements.size() != count)
        throw std::invalid_argument{
            std::string{what} + " expects " + std::to_string(count) + " values, found "
            + std::to_string(elements.size())};
    return elements;
}

template<class Cost>
inline constexpr bool text_readable_v = arithmetic<Cost>;

template<class HardCost, class SoftCost>
inline constexpr bool text_readable_v<hierarchical<HardCost, SoftCost>> =
    text_readable_v<HardCost> && text_readable_v<SoftCost>;

template<class... Values>
inline constexpr bool text_readable_v<lexicographic<Values...>> =
    (text_readable_v<Values> && ...);

template<class... Values>
inline constexpr bool text_readable_v<pareto<Values...>> =
    (text_readable_v<Values> && ...);

} // namespace detail

/// Costs that from_text reads: arithmetic ones, and hierarchical, lexicographic
/// and pareto costs of them.
///
/// Others need the problem's read_cost.
template<class Cost>
concept text_readable = detail::text_readable_v<std::remove_cv_t<Cost>>;

/// The cost written by text: throws std::invalid_argument, with the reason,
/// when the text does not describe a value of the type.
template<text_readable Cost>
[[nodiscard]]
Cost from_text(std::string_view text)
{
    using cost_type = std::remove_cv_t<Cost>;
    text = easylocal::detail::trim_space(text);

    if constexpr (arithmetic<cost_type>)
    {
        const auto value = easylocal::detail::parse_number<cost_type>(text);
        if (!value)
            throw std::invalid_argument{
                std::string{
                    std::floating_point<cost_type>
                        ? "expected a number"
                        : "expected an integer"}
                + ", found '" + std::string{text} + "'"};
        // NaN is no cost: it compares with nothing.
        if constexpr (std::floating_point<cost_type>)
        {
            if (!(*value == *value))
                throw std::invalid_argument{"expected a number, found 'nan'"};
        }
        return *value;
    }
    else if constexpr (hierarchical_type<cost_type>)
    {
        const auto elements =
            detail::elements_of(text, 2, "a hierarchical cost [hard, soft]");
        return cost_type{
            from_text<typename cost_type::hard_cost_type>(elements[0]),
            from_text<typename cost_type::soft_cost_type>(elements[1])};
    }
    else
    {
        // A lexicographic or pareto cost, the only other text_readable ones.
        return
            []<template<class...> class Levels, class... Values>(
                std::type_identity<Levels<Values...>>,
                const std::vector<std::string_view>& elements) {
                return [&]<std::size_t... Index>(std::index_sequence<Index...>) {
                    return Levels<Values...>{from_text<Values>(elements[Index])...};
                }(std::index_sequence_for<Values...>{});
            }(std::type_identity<cost_type>{},
                detail::elements_of(
                    text,
                    cost_type::levels,
                    pareto_type<cost_type> ? "a pareto cost" : "a lexicographic cost"));
    }
}

/// A cost in the text from_text reads back: numbers in their shortest exact
/// form, [hard, soft] and [v1, v2, ...] for structured costs.
template<text_readable Cost>
[[nodiscard]]
std::string to_text(const Cost& value)
{
    using cost_type = std::remove_cv_t<Cost>;
    if constexpr (arithmetic<cost_type>)
    {
        return easylocal::detail::number_text(value);
    }
    else if constexpr (hierarchical_type<cost_type>)
    {
        return "[" + to_text(value.hard()) + ", " + to_text(value.soft()) + "]";
    }
    else
    {
        std::string text{"["};
        [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            ((text += (Index == 0 ? "" : ", ") + to_text(value.template get<Index>())),
                ...);
        }(std::make_index_sequence<cost_type::levels>{});
        return text + "]";
    }
}

} // namespace easylocal::cost
