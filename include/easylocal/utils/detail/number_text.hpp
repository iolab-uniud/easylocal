#pragma once

// A number as text, and back, for the formats EasyLocal reads back:
// configuration values, costs, TOML and REST overrides.

#include <easylocal/utils/detail/text.hpp>

#include <array>
#include <charconv>
#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace easylocal::detail
{

// An integer or a floating-point number, not a bool.
template<class T>
concept number =
    (std::integral<std::remove_cv_t<T>> || std::floating_point<std::remove_cv_t<T>>)
    && !std::same_as<std::remove_cv_t<T>, bool>;

// The shortest text that reads back to the same value (std::to_chars).
template<number Number>
[[nodiscard]]
std::string number_text(const Number value)
{
    std::array<char, 64> buffer{};
    const auto [end, error] =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    static_cast<void>(error); // 64 characters hold any arithmetic value
    return std::string{buffer.data(), end};
}

// The number the text writes, spaces around it allowed (std::from_chars, in
// base 10 or in the general floating-point format): nothing when the text is
// not one, or has more.
template<number Number>
[[nodiscard]]
std::optional<Number> parse_number(std::string_view text) noexcept
{
    text = trim_space(text);
    Number value{};
    const auto* const first = text.data();
    const auto* const last = first + text.size();
    const auto [end, error] = [&] {
        if constexpr (std::floating_point<Number>)
            return std::from_chars(first, last, value, std::chars_format::general);
        else
            return std::from_chars(first, last, value, 10);
    }();
    if (text.empty() || error != std::errc{} || end != last)
        return std::nullopt;
    return value;
}

} // namespace easylocal::detail
