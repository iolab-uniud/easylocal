#pragma once

// A number as text, for the formats EasyLocal reads back: configuration
// values, costs, TOML and REST overrides.

#include <array>
#include <charconv>
#include <concepts>
#include <string>
#include <type_traits>

namespace easylocal::detail
{

// The shortest text that reads back to the same value (std::to_chars).
template<class Number>
    requires(std::integral<Number> || std::floating_point<Number>)
    && (!std::same_as<std::remove_cv_t<Number>, bool>)
[[nodiscard]]
std::string number_text(const Number value)
{
    std::array<char, 64> buffer{};
    const auto [end, error] =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    static_cast<void>(error); // 64 characters hold any arithmetic value
    return std::string{buffer.data(), end};
}

} // namespace easylocal::detail
