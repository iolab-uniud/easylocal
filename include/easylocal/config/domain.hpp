#pragma once

/// \file
/// The domains of parameters: the values a field may take, declared in its
/// schema with config::range or config::one_of.
///
/// A domain is checked when the parameters are validated, and exported to
/// automatic configurators such as irace.

#include <easylocal/utils/detail/number_text.hpp>
#include <easylocal/utils/limit.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::config
{

/// No domain: a field may take any value of its type.
struct no_domain
{
};

/// A range of numbers, closed unless open(), open_low() or open_high() says
/// otherwise, and sampled on a logarithmic scale by a configurator after log().
///
/// It is made with config::range.
template<class Number>
struct range_domain
{
    /// The lower bound.
    Number low{};
    /// The upper bound.
    Number high{};
    /// Whether the lower bound is excluded.
    bool low_open{false};
    /// Whether the upper bound is excluded.
    bool high_open{false};
    /// Whether a configurator samples the range on a logarithmic scale.
    bool logarithmic{false};

    /// The same range without its bounds.
    [[nodiscard]]
    constexpr range_domain open() const noexcept
    {
        auto result = *this;
        result.low_open = true;
        result.high_open = true;
        return result;
    }

    /// The same range without its lower bound.
    [[nodiscard]]
    constexpr range_domain open_low() const noexcept
    {
        auto result = *this;
        result.low_open = true;
        return result;
    }

    /// The same range without its upper bound.
    [[nodiscard]]
    constexpr range_domain open_high() const noexcept
    {
        auto result = *this;
        result.high_open = true;
        return result;
    }

    /// The same range on a logarithmic scale; its lower bound must be positive.
    ///
    /// Throws std::invalid_argument otherwise (a compilation error in a schema).
    [[nodiscard]]
    constexpr range_domain log() const
    {
        if (!(low > Number{}))
            throw std::invalid_argument{
                "a logarithmic range needs a positive lower bound"};
        auto result = *this;
        result.logarithmic = true;
        return result;
    }
};

/// The numbers from low to high, bounds included, as the domain of a numeric
/// field: `config::range(0.0, 1.0).open()` is (0, 1).
///
/// Throws std::invalid_argument when low is not below high (a compilation error
/// in a schema).
template<class Number>
    requires(std::integral<Number> || std::floating_point<Number>)
    && (!std::same_as<Number, bool>)
[[nodiscard]]
constexpr range_domain<Number> range(const Number low, const Number high)
{
    if (!(low < high))
        throw std::invalid_argument{"a range needs its lower bound below the upper one"};
    return {.low = low, .high = high};
}

/// A set of values, as the domain of a text or numeric field.
///
/// It is made with config::one_of.
template<class Value, std::size_t Size>
struct choice_domain
{
    /// The type of the values.
    using value_type = Value;

    /// The values.
    std::array<Value, Size> values{};
};

namespace detail
{

template<class Value>
using choice_value_t = std::conditional_t<
    std::convertible_to<Value, std::string_view>,
    std::string_view,
    std::remove_cvref_t<Value>>;

} // namespace detail

/// The values given, as the domain of a field: `config::one_of("tabu",
/// "random")` for text, `config::one_of(10, 100, 1000)` for numbers.
template<class First, class... Rest>
[[nodiscard]]
constexpr auto one_of(const First& first, const Rest&... rest)
{
    using value_type = detail::choice_value_t<First>;
    return choice_domain<value_type, 1 + sizeof...(Rest)>{
        .values = {value_type(first), value_type(rest)...}};
}

/// Whether a type is a domain: no_domain, a range_domain or a choice_domain.
template<class Domain>
inline constexpr bool is_domain_v = std::same_as<Domain, no_domain>;

/// A range_domain is one.
template<class Number>
inline constexpr bool is_domain_v<range_domain<Number>> = true;

/// A choice_domain is one.
template<class Value, std::size_t Size>
inline constexpr bool is_domain_v<choice_domain<Value, Size>> = true;

namespace detail
{

template<class T>
inline constexpr bool is_range_domain_v = false;

template<class Number>
inline constexpr bool is_range_domain_v<range_domain<Number>> = true;

template<class T>
inline constexpr bool is_choice_domain_v = false;

template<class Value, std::size_t Size>
inline constexpr bool is_choice_domain_v<choice_domain<Value, Size>> = true;

template<class T>
inline constexpr bool is_number_v =
    (std::integral<T> || std::floating_point<T>) && !std::same_as<T, bool>;

template<class T>
struct domain_element
{
    using type = T;
};

template<class T, std::size_t Size>
struct domain_element<std::array<T, Size>>
{
    using type = T;
};

template<class T, class Allocator>
struct domain_element<std::vector<T, Allocator>>
{
    using type = T;
};

// The values a domain checks: the elements of an array or a vector, the field
// itself otherwise.
template<class T>
using domain_element_t = typename domain_element<std::remove_cv_t<T>>::type;

template<class Left, class Right>
[[nodiscard]]
constexpr bool number_less(const Left left, const Right right) noexcept
{
    if constexpr (std::integral<Left> && std::integral<Right>)
        return std::cmp_less(left, right);
    else
        return static_cast<long double>(left) < static_cast<long double>(right);
}

template<class Left, class Right>
[[nodiscard]]
constexpr bool number_equal(const Left left, const Right right) noexcept
{
    if constexpr (std::integral<Left> && std::integral<Right>)
        return std::cmp_equal(left, right);
    else
        return static_cast<long double>(left) == static_cast<long double>(right);
}

template<class Number, class Value>
[[nodiscard]]
constexpr bool range_contains(
    const range_domain<Number>& domain,
    const Value value) noexcept
{
    if constexpr (std::same_as<Value, easylocal::limit>)
    {
        return !value.is_unlimited()
            && range_contains(domain, static_cast<std::size_t>(value));
    }
    else
    {
        if constexpr (std::floating_point<Value>)
        {
            if (!(value == value)) // NaN
                return false;
        }
        const bool above = domain.low_open
            ? number_less(domain.low, value)
            : !number_less(value, domain.low);
        const bool below = domain.high_open
            ? number_less(value, domain.high)
            : !number_less(domain.high, value);
        return above && below;
    }
}

template<class Choice, std::size_t Size, class Value>
[[nodiscard]]
constexpr bool choice_contains(
    const choice_domain<Choice, Size>& domain,
    const Value& value) noexcept
{
    for (const auto& choice : domain.values)
    {
        if constexpr (std::same_as<Choice, std::string_view>)
        {
            if (std::string_view{value} == choice)
                return true;
        }
        else if (number_equal(choice, value))
            return true;
    }
    return false;
}

} // namespace detail

/// Whether a domain can be declared for a field of type Value: a range for a
/// number or a limit, a set of text values for text, a set of numbers for a
/// number; for an array or a vector, the domain of its elements.
template<class Domain, class Value>
concept domain_for = std::same_as<Domain, no_domain>
    || (detail::is_range_domain_v<Domain>
        && (detail::is_number_v<detail::domain_element_t<Value>>
            || std::same_as<detail::domain_element_t<Value>, easylocal::limit>))
    || (detail::is_choice_domain_v<Domain>
        && ((std::same_as<typename Domain::value_type, std::string_view>
                && std::same_as<detail::domain_element_t<Value>, std::string>)
            || (detail::is_number_v<typename Domain::value_type>
                && detail::is_number_v<detail::domain_element_t<Value>>)));

/// Whether value lies in domain; for an array or a vector, whether each element
/// does. Every value lies in no_domain.
template<class Domain, class Value>
    requires domain_for<Domain, Value>
[[nodiscard]]
constexpr bool domain_contains(const Domain& domain, const Value& value) noexcept
{
    if constexpr (std::same_as<Domain, no_domain>)
        return true;
    else if constexpr (!std::same_as<detail::domain_element_t<Value>, Value>)
    {
        for (const auto& element : value)
            if (!domain_contains(domain, element))
                return false;
        return true;
    }
    else if constexpr (detail::is_range_domain_v<Domain>)
        return detail::range_contains(domain, value);
    else
        return detail::choice_contains(domain, value);
}

/// The kind of a parameter's value, as a configurator types it.
enum class parameter_kind
{
    /// true or false.
    boolean,
    /// An integer.
    integer,
    /// A floating-point number.
    real,
    /// A count or "unlimited" (an easylocal::limit).
    limit,
    /// Text.
    text,
    /// A file path.
    path,
    /// An array or a vector of values.
    list,
};

/// The kind of a value of type Value.
template<class Value>
[[nodiscard]]
consteval parameter_kind kind_of() noexcept
{
    using value_type = std::remove_cvref_t<Value>;
    if constexpr (std::same_as<value_type, bool>)
        return parameter_kind::boolean;
    else if constexpr (std::integral<value_type>)
        return parameter_kind::integer;
    else if constexpr (std::floating_point<value_type>)
        return parameter_kind::real;
    else if constexpr (std::same_as<value_type, easylocal::limit>)
        return parameter_kind::limit;
    else if constexpr (std::same_as<value_type, std::filesystem::path>)
        return parameter_kind::path;
    else if constexpr (std::same_as<value_type, std::string>)
        return parameter_kind::text;
    else
        return parameter_kind::list;
}

/// A domain at run time, whatever its type: what a parameter_set lists and a
/// configurator exporter reads.
struct domain_info
{
    /// The shape of a domain.
    enum class shape
    {
        /// No domain.
        none,
        /// A range of numbers.
        range,
        /// A set of values.
        choice,
    };

    /// The shape of the domain.
    shape kind{shape::none};
    /// The lower bound of a range.
    double low{};
    /// The upper bound of a range.
    double high{};
    /// The lower bound of a range, as text in the syntax of the field.
    std::string low_text{};
    /// The upper bound of a range, as text in the syntax of the field.
    std::string high_text{};
    /// Whether the lower bound of a range is excluded.
    bool low_open{false};
    /// Whether the upper bound of a range is excluded.
    bool high_open{false};
    /// Whether a configurator samples the range on a logarithmic scale.
    bool logarithmic{false};
    /// The values of a set, as text.
    std::vector<std::string> choices{};

    /// Whether there is a domain.
    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return kind != shape::none;
    }

    /// The domain as text: "(0, 1]", "[1, 1000] log", "{tabu, random}"; empty
    /// for no domain.
    [[nodiscard]]
    std::string text() const
    {
        std::string result;
        if (kind == shape::range)
        {
            result += low_open ? '(' : '[';
            result += low_text;
            result += ", ";
            result += high_text;
            result += high_open ? ')' : ']';
            if (logarithmic)
                result += " log";
        }
        else if (kind == shape::choice)
        {
            result += '{';
            for (std::size_t index = 0; index < choices.size(); ++index)
            {
                if (index != 0)
                    result += ", ";
                result += choices[index];
            }
            result += '}';
        }
        return result;
    }

    /// Whether every value of other lies in this domain: a range within this
    /// range, values within this set or this range. Everything lies within no
    /// domain.
    [[nodiscard]]
    bool contains(const domain_info& other) const
    {
        if (kind == shape::none)
            return true;
        if (other.kind == shape::none)
            return false;
        const auto contains_number = [this](const double value) {
            const bool above = low_open ? low < value : low <= value;
            const bool below = high_open ? value < high : value <= high;
            return above && below;
        };
        if (kind == shape::range)
        {
            if (other.kind == shape::range)
            {
                const bool above = low < other.low
                    || (low == other.low && (!low_open || other.low_open));
                const bool below = other.high < high
                    || (other.high == high && (!high_open || other.high_open));
                return above && below;
            }
            for (const auto& choice : other.choices)
            {
                try
                {
                    if (!contains_number(std::stod(choice)))
                        return false;
                }
                catch (const std::exception&)
                {
                    return false;
                }
            }
            return true;
        }
        if (other.kind == shape::range)
            return false;
        for (const auto& choice : other.choices)
        {
            bool found = false;
            for (const auto& present : choices)
                found = found || present == choice;
            if (!found)
                return false;
        }
        return true;
    }
};

/// The run-time description of a domain.
template<class Domain>
    requires is_domain_v<Domain>
[[nodiscard]]
domain_info describe_domain(const Domain& domain)
{
    domain_info result;
    if constexpr (detail::is_range_domain_v<Domain>)
    {
        result.kind = domain_info::shape::range;
        result.low = static_cast<double>(domain.low);
        result.high = static_cast<double>(domain.high);
        result.low_text = easylocal::detail::number_text(domain.low);
        result.high_text = easylocal::detail::number_text(domain.high);
        result.low_open = domain.low_open;
        result.high_open = domain.high_open;
        result.logarithmic = domain.logarithmic;
    }
    else if constexpr (detail::is_choice_domain_v<Domain>)
    {
        result.kind = domain_info::shape::choice;
        for (const auto& value : domain.values)
            if constexpr (std::same_as<
                              std::remove_cvref_t<decltype(value)>,
                              std::string_view>)
                result.choices.emplace_back(value);
            else
                result.choices.push_back(easylocal::detail::number_text(value));
    }
    return result;
}

} // namespace easylocal::config
