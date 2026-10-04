#pragma once

// Text of the formats EasyLocal reads back (configuration values, costs, TOML
// and REST overrides): trimming, and the elements of a list.

#include <cstddef>
#include <string_view>
#include <vector>

namespace easylocal::detail
{

// The text without the spaces, tabs and line ends around it.
[[nodiscard]]
constexpr std::string_view trim_space(const std::string_view text) noexcept
{
    constexpr std::string_view space{" \t\n\r"};
    const auto first = text.find_first_not_of(space);
    if (first == std::string_view::npos)
        return {};
    const auto last = text.find_last_not_of(space);
    return text.substr(first, last - first + 1);
}

// The elements of the body of a list, "a, [b, c], d" without its brackets: split
// at the commas outside nested brackets, each trimmed. A blank body has none.
[[nodiscard]]
inline std::vector<std::string_view> split_list(const std::string_view body)
{
    std::vector<std::string_view> elements;
    if (trim_space(body).empty())
        return elements;
    std::size_t depth = 0;
    std::size_t start = 0;
    for (std::size_t index = 0; index <= body.size(); ++index)
        if (index == body.size() || (body[index] == ',' && depth == 0))
        {
            elements.push_back(trim_space(body.substr(start, index - start)));
            start = index + 1;
        }
        else if (body[index] == '[')
            ++depth;
        else if (body[index] == ']' && depth > 0)
            --depth;
    return elements;
}

} // namespace easylocal::detail
