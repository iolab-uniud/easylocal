#pragma once

// Costs written as text, for targets given on the command line, in files or in
// the TextUI: a number for an arithmetic cost, [hard, soft] for a
// cost::hierarchical, [v1, v2, ...] for a cost::lexicographic, nested as the
// types are (for example [0, [3, 1.5]]).

#include <easylocal/cost/concepts.hpp>
#include <easylocal/cost/hierarchical.hpp>
#include <easylocal/cost/lexicographic.hpp>

#include <charconv>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::cost
{

namespace detail
{

[[nodiscard]]
constexpr std::string_view trim_text(std::string_view text) noexcept
{
    constexpr std::string_view space{" \t\n\r"};
    const auto first = text.find_first_not_of(space);
    if (first == std::string_view::npos)
        return {};
    const auto last = text.find_last_not_of(space);
    return text.substr(first, last - first + 1);
}

// The elements of "[a, b, ...]", split at the commas outside nested brackets.
[[nodiscard]]
inline std::vector<std::string_view> bracketed_elements(std::string_view text)
{
    text = trim_text(text);
    if (text.size() < 2 || text.front() != '[' || text.back() != ']')
        throw std::invalid_argument{"expected [...], found '" + std::string{text} + "'"};
    text = text.substr(1, text.size() - 2);

    std::vector<std::string_view> elements;
    std::size_t depth = 0;
    std::size_t start = 0;
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        if (text[index] == '[')
            ++depth;
        else if (text[index] == ']' && depth > 0)
            --depth;
        else if (text[index] == ',' && depth == 0)
        {
            elements.push_back(text.substr(start, index - start));
            start = index + 1;
        }
    }
    elements.push_back(text.substr(start));
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

} // namespace detail

// Costs that from_text reads: arithmetic ones, and hierarchical and
// lexicographic costs of them. Others need the problem's read_cost.
template<class Cost>
concept text_readable = detail::text_readable_v<std::remove_cv_t<Cost>>;

// The cost written by text: throws std::invalid_argument, with the reason,
// when the text does not describe a value of the type.
template<text_readable Cost>
[[nodiscard]]
Cost from_text(std::string_view text)
{
    using cost_type = std::remove_cv_t<Cost>;
    text = detail::trim_text(text);

    if constexpr (arithmetic<cost_type>)
    {
        cost_type value{};
        const auto* const first = text.data();
        const auto* const last = first + text.size();
        const auto [end, error] = [&] {
            if constexpr (std::floating_point<cost_type>)
                return std::from_chars(first, last, value, std::chars_format::general);
            else
                return std::from_chars(first, last, value);
        }();
        if (text.empty() || error != std::errc{} || end != last)
            throw std::invalid_argument{
                std::string{
                    std::floating_point<cost_type>
                        ? "expected a number"
                        : "expected an integer"}
                + ", found '" + std::string{text} + "'"};
        return value;
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
        // A lexicographic cost, the only other text_readable one.
        return []<class... Values>(
                   std::type_identity<lexicographic<Values...>>,
                   const std::vector<std::string_view>& elements) {
            return [&]<std::size_t... Index>(std::index_sequence<Index...>) {
                return lexicographic<Values...>{from_text<Values>(elements[Index])...};
            }(std::index_sequence_for<Values...>{});
        }(std::type_identity<cost_type>{},
                   detail::elements_of(
                       text,
                       lexicographic_traits<cost_type>::size,
                       "a lexicographic cost"));
    }
}

} // namespace easylocal::cost
