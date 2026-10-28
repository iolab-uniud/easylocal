#pragma once

/// \file
/// parameter: the value of one field of a parameter block, which may not have
/// been set.
///
/// A field declares its default with its member initializer; a field without
/// one is set by nothing and reading it throws parameter_not_set.

#include <easylocal/config/fixed_string.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace easylocal::config
{

/// Reading a parameter that nothing has set and that has no default.
///
/// The message names the parameter.
class parameter_not_set : public std::logic_error
{
public:
    /// From the name of the parameter.
    explicit parameter_not_set(const std::string_view name)
        : std::logic_error{
              "parameter " + std::string{name} + " was not set, and it has no default"}
    {
    }
};

/// The value of one field of a parameter block: a Value named Name, which may
/// not have been set.
///
/// A member initializer is the default of the field, `parameter<double,
/// "cooling_rate"> cooling_rate{0.95}`; a field declared without one is
/// required, and reading it before anything sets it throws parameter_not_set.
/// It converts to its value, so a parameter reads like the value it holds;
/// where a template deduces its argument, value() spells the value out.
template<class Value, fixed_string Name>
class parameter
{
public:
    /// The type of the value.
    using value_type = Value;

    /// Not set: the field is required, unless a member initializer gives it a
    /// default.
    parameter() = default;

    /// From a value, which the field is then set to: the member initializer of
    /// a field is its default.
    constexpr parameter(Value value) noexcept(std::is_nothrow_move_constructible_v<Value>)
        : value_{std::move(value)}
    {
    }

    /// Sets the field to a value.
    constexpr parameter& operator=(Value value) noexcept(
        std::is_nothrow_move_constructible_v<Value>)
    {
        value_ = std::move(value);
        return *this;
    }

    /// Whether anything has set the field, its default included.
    [[nodiscard]]
    constexpr bool is_set() const noexcept
    {
        return value_.has_value();
    }

    /// The value of the field; throws parameter_not_set when nothing has set
    /// it.
    [[nodiscard]]
    constexpr const Value& value() const
    {
        if (!value_.has_value())
            throw parameter_not_set{Name.view()};
        return *value_;
    }

    /// The value of the field, where a value is expected; throws
    /// parameter_not_set when nothing has set it.
    [[nodiscard]]
    constexpr operator const Value&() const
    {
        return value();
    }

    /// The name of the field.
    [[nodiscard]]
    static constexpr std::string_view name() noexcept
    {
        return Name.view();
    }

private:
    std::optional<Value> value_{};
};

namespace detail
{

// Whether a type is a parameter.
template<class T>
inline constexpr bool is_parameter_v = false;

template<class Value, fixed_string Name>
inline constexpr bool is_parameter_v<parameter<Value, Name>> = true;

// The type of the value of a parameter, and the type itself for anything else.
template<class T>
struct parameter_value
{
    using type = T;
};

template<class Value, fixed_string Name>
struct parameter_value<parameter<Value, Name>>
{
    using type = Value;
};

template<class T>
using parameter_value_t = typename parameter_value<T>::type;

} // namespace detail

} // namespace easylocal::config
