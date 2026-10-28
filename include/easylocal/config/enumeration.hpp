#pragma once

/// \file
/// enumeration: the names the values of an enum take in a configuration,
/// declared once beside the enum.
///
/// A field of an enum type is then read and written by name, on the command
/// line, in a configuration file, in TOML, in the TextUI and in an irace
/// scenario, and its domain is the values declared.

#include <array>
#include <concepts>
#include <cstddef>
#include <optional>
#include <string_view>
#include <type_traits>

namespace easylocal
{

/// One value of an enumeration and the name it takes in a configuration.
template<class Enumeration>
struct enumerator
{
    /// The name of the value, as a configurator writes it.
    std::string_view name;
    /// The value.
    Enumeration value;
};

/// The names of the values of an enumeration, which a specialization declares:
/// a `values` of easylocal::enumerator, in the order a configurator sees them.
///
/// C++ cannot read the names of an enum, so they are declared once, beside it:
///
/// \code
/// enum class Start { random, initial };
///
/// template<>
/// struct easylocal::enumeration<Start>
/// {
///     static constexpr std::array values{
///         easylocal::enumerator{"random", Start::random},
///         easylocal::enumerator{"initial", Start::initial}};
/// };
/// \endcode
///
/// The primary template is not defined: a field of an enum without one does
/// not compile.
template<class Enumeration>
struct enumeration;

/// An enum whose values are named: an easylocal::enumeration declares them.
template<class T>
concept enumerated = std::is_enum_v<T> && requires { enumeration<T>::values; };

} // namespace easylocal

namespace easylocal::config::detail
{

// The name of a value, empty when it is none of the declared ones.
template<easylocal::enumerated Enumeration>
[[nodiscard]]
constexpr std::string_view enumerator_name(const Enumeration value) noexcept
{
    for (const auto& enumerator : easylocal::enumeration<Enumeration>::values)
        if (enumerator.value == value)
            return enumerator.name;
    return {};
}

// The value of a name, empty when no declared value has it.
template<easylocal::enumerated Enumeration>
[[nodiscard]]
constexpr std::optional<Enumeration> enumerator_value(
    const std::string_view name) noexcept
{
    for (const auto& enumerator : easylocal::enumeration<Enumeration>::values)
        if (enumerator.name == name)
            return enumerator.value;
    return {};
}

// Whether the value is one of the declared ones.
template<easylocal::enumerated Enumeration>
[[nodiscard]]
constexpr bool is_declared(const Enumeration value) noexcept
{
    return !enumerator_name(value).empty();
}

// The number of values an enumeration declares.
template<easylocal::enumerated Enumeration>
inline constexpr std::size_t enumerator_count =
    easylocal::enumeration<Enumeration>::values.size();

// The values an enumeration declares, in the order it declares them.
template<easylocal::enumerated Enumeration>
[[nodiscard]]
consteval auto enumerator_values() noexcept
{
    std::array<Enumeration, enumerator_count<Enumeration>> values{};
    std::size_t index = 0;
    for (const auto& enumerator : easylocal::enumeration<Enumeration>::values)
        values[index++] = enumerator.value;
    return values;
}

} // namespace easylocal::config::detail
