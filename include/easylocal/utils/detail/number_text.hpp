#pragma once

// A number as text, and back, for the formats EasyLocal reads back:
// configuration values, costs, TOML and REST overrides.

#include <easylocal/utils/detail/text.hpp>

#include <array>
#include <charconv>
#include <concepts>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
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

// A floating-point type without std::from_chars: long double with libc++.
template<class Number>
concept without_from_chars =
    std::floating_point<Number> && !requires(const char* text, Number& value) {
        std::from_chars(text, text, value, std::chars_format::general);
    };

// The number the text writes, spaces around it allowed (std::from_chars, in
// base 10 or in the general floating-point format): nothing when the text is
// not one, or has more.
//
// A long double where the library has no std::from_chars for it (libc++) is
// read through double when the two have the same mantissa (Apple's arm64),
// else with a stream in the classic locale, which reads it as strtold does.
template<number Number>
[[nodiscard]]
std::optional<Number> parse_number(std::string_view text) noexcept
{
    text = trim_space(text);
    const auto* const first = text.data();
    const auto* const last = first + text.size();
    if constexpr (without_from_chars<Number>)
    {
        if constexpr (std::numeric_limits<Number>::digits
            == std::numeric_limits<double>::digits)
        {
            const auto value = parse_number<double>(text);
            if (!value)
                return std::nullopt;
            return static_cast<Number>(*value);
        }
        else
        {
            // A plus sign, or a hexadecimal number, is not what from_chars
            // reads.
            if (text.empty() || text.front() == '+'
                || text.find_first_of("xX") != std::string_view::npos)
                return std::nullopt;
            try
            {
                std::istringstream in{std::string{text}};
                in.imbue(std::locale::classic());
                Number value{};
                in >> value;
                if (in.fail() || in.peek() != std::char_traits<char>::eof())
                    return std::nullopt;
                return value;
            }
            catch (...)
            {
                return std::nullopt;
            }
        }
    }
    else
    {
        Number value{};
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
}

} // namespace easylocal::detail
