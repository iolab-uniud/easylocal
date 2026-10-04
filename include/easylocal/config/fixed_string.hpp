#pragma once

/// \file
/// fixed_string: a string literal as a template argument, for the names of
/// parameters and the paths that expressions refer to.

#include <cstddef>
#include <string_view>

namespace easylocal::config
{

/// A string literal as a template argument: the name of a field or of a group.
template<std::size_t Size>
struct fixed_string
{
    /// The characters, with the terminating null.
    char value[Size]{};

    /// From a string literal.
    consteval fixed_string(const char (&text)[Size])
    {
        for (std::size_t index = 0; index < Size; ++index)
            value[index] = text[index];
    }

    /// The characters, without the terminating null.
    [[nodiscard]]
    constexpr std::string_view view() const noexcept
    {
        static_assert(Size >= 1);
        return {value, Size - 1};
    }
};

/// Deduces the size from the string literal.
template<std::size_t Size>
fixed_string(const char (&)[Size]) -> fixed_string<Size>;

} // namespace easylocal::config
