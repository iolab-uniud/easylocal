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
/// otherwise, and sampled on a logarithmic scale by a configurator after log();
/// its upper bound may be unlimited.
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
    /// Whether the range has no upper bound: high is then not used, and a
    /// limit may be unlimited.
    bool unbounded{false};

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
    /// Throws std::invalid_argument otherwise (a compilation error in a
    /// schema).
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
/// in a schema). Requires an integer or floating-point type, not bool.
template<easylocal::detail::number Number>
[[nodiscard]]
constexpr range_domain<Number> range(const Number low, const Number high)
{
    if (!(low < high))
        throw std::invalid_argument{"a range needs its lower bound below the upper one"};
    return {.low = low, .high = high};
}

/// The numbers from low up, with no upper bound: `config::range(0.0,
/// easylocal::unlimited).open_low()` is the positive numbers (infinity
/// included), `range(1, unlimited)` a count of at least one, or unlimited for
/// a limit.
///
/// Throws std::invalid_argument when high is a count rather than unlimited (a
/// compilation error in a schema). Requires an integer or floating-point type,
/// not bool.
template<easylocal::detail::number Number>
[[nodiscard]]
constexpr range_domain<Number> range(const Number low, const easylocal::limit high)
{
    if (!high.is_unlimited())
        throw std::invalid_argument{"a range's upper bound is a number or unlimited"};
    return {.low = low, .high = Number{}, .unbounded = true};
}

/// Any value: the domain of a field whose values are all valid, such as a seed,
/// free text or a number that may be negative. It is declared as
/// easylocal::unlimited: `field<"seed", &P::seed>("...",
/// easylocal::unlimited)`.
struct unbounded_domain
{
};

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

// The type of the values of a choice: text as std::string_view, numbers as
// their common type (one_of(1, 1.5) holds doubles), anything else as the type
// of the first value.
template<class First, class... Rest>
struct choice_value
{
    using type = std::conditional_t<
        std::convertible_to<First, std::string_view>,
        std::string_view,
        std::remove_cvref_t<First>>;
};

template<easylocal::detail::number First, easylocal::detail::number... Rest>
struct choice_value<First, Rest...>
{
    using type = std::common_type_t<First, Rest...>;
};

template<class First, class... Rest>
using choice_value_t = typename choice_value<First, Rest...>::type;

} // namespace detail

/// The values given, as the domain of a field: `config::one_of("tabu",
/// "random")` for text, `config::one_of(10, 100, 1000)` for numbers.
///
/// Numbers of different types are held as their common type, as
/// `one_of(1, 1.5, 2)` holds 1.0, 1.5 and 2.0.
template<class First, class... Rest>
[[nodiscard]]
constexpr auto one_of(const First& first, const Rest&... rest)
{
    using value_type = detail::choice_value_t<First, Rest...>;
    return choice_domain<value_type, 1 + sizeof...(Rest)>{
        .values = {value_type(first), value_type(rest)...}};
}

/// Whether a type is a domain: no_domain, unbounded_domain, a range_domain or a
/// choice_domain.
template<class Domain>
inline constexpr bool is_domain_v =
    std::same_as<Domain, no_domain> || std::same_as<Domain, unbounded_domain>;

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
inline constexpr bool is_number_v = easylocal::detail::number<T>;

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
        if (value.is_unlimited())
            return domain.unbounded;
        return range_contains(domain, static_cast<std::size_t>(value));
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
        if (domain.unbounded)
            return above;
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
concept domain_for =
    std::same_as<Domain, no_domain> || std::same_as<Domain, unbounded_domain>
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
    if constexpr (std::same_as<Domain, no_domain>
        || std::same_as<Domain, unbounded_domain>)
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
        /// Any value.
        unbounded,
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
    /// Whether a range has no upper bound.
    bool high_unlimited{false};
    /// The values of a set, as text.
    std::vector<std::string> choices{};

    /// Whether there is a domain.
    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return kind != shape::none;
    }

    /// The domain as text: "(0, 1]", "[1, 1000] log", "[1, unlimited)",
    /// "{tabu, random}", "unlimited" for any value; empty for no domain.
    [[nodiscard]]
    std::string text() const
    {
        std::string result;
        if (kind == shape::range)
        {
            result += low_open ? '(' : '[';
            result += low_text;
            result += ", ";
            result += high_unlimited ? "unlimited" : high_text;
            result += high_open || high_unlimited ? ')' : ']';
            if (logarithmic)
                result += " log";
        }
        else if (kind == shape::unbounded)
            result = "unlimited";
        else if (kind == shape::choice)
        {
            result += '{';
            for (std::size_t index = 0; index < choices.size(); ++index)
            {
                if (index != 0)
                    result += ", ";
                result += choices[index].empty() ? "\"\"" : choices[index];
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
        if (kind == shape::none || kind == shape::unbounded)
            return true;
        if (other.kind == shape::none || other.kind == shape::unbounded)
            return false;
        const auto contains_number = [this](const double value) {
            const bool above = low_open ? low < value : low <= value;
            const bool below =
                high_unlimited || (high_open ? value < high : value <= high);
            return above && below;
        };
        if (kind == shape::range)
        {
            if (other.kind == shape::range)
            {
                const bool above = low < other.low
                    || (low == other.low && (!low_open || other.low_open));
                const bool below = high_unlimited
                    || (!other.high_unlimited
                        && (other.high < high
                            || (other.high == high && (!high_open || other.high_open))));
                return above && below;
            }
            for (const auto& choice : other.choices)
            {
                const auto number = easylocal::detail::parse_number<double>(choice);
                if (!number || !contains_number(*number))
                    return false;
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
        result.high_unlimited = domain.unbounded;
        if (domain.unbounded)
            result.high_text = "unlimited";
    }
    else if constexpr (std::same_as<Domain, unbounded_domain>)
        result.kind = domain_info::shape::unbounded;
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
