#pragma once

// Text of the formats EasyLocal reads back (configuration values, costs, TOML
// and REST overrides): trimming, the elements of a list, and paths as UTF-8.

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace easylocal::detail
{

// The path that a UTF-8 text names, as TOML, REST and the TextUI give it: on
// Windows, path(std::string) reads the text in the ANSI code page.
[[nodiscard]]
inline std::filesystem::path path_from_utf8(const std::string_view text)
{
    return std::filesystem::path{std::u8string{text.begin(), text.end()}};
}

// The text of a path in UTF-8: on Windows, path::string() writes it in the ANSI
// code page, and throws for a character the code page lacks.
[[nodiscard]]
inline std::string utf8_text(const std::filesystem::path& path)
{
    const auto text = path.u8string();
    return std::string{text.begin(), text.end()};
}

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
