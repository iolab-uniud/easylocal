#pragma once

// The text of a value for people, from its optional hooks: a member
// value.describe(), a free describe(value) found by ADL, or operator<<, in
// this order. easylocal::describe (app/io.hpp) is built on it, and the
// contract checks (testing/) name the moves and values of their failures
// with it.

#include <concepts>
#include <ostream>
#include <sstream>
#include <string>

namespace easylocal::detail::io
{

namespace adl
{

void describe() = delete;

template<class T>
concept has_describe = requires(const T& value) {
    { describe(value) } -> std::convertible_to<std::string>;
};

template<class T>
    requires has_describe<T>
[[nodiscard]]
std::string call_describe(const T& value)
{
    return describe(value);
}

} // namespace adl

template<class T>
concept member_describable = requires(const T& value) {
    { value.describe() } -> std::convertible_to<std::string>;
};

template<class T>
concept ostream_insertable = requires(std::ostream& out, const T& value) {
    out << value;
};

// A value with one of the hooks.
template<class T>
concept describable_value =
    member_describable<T> || adl::has_describe<T> || ostream_insertable<T>;

// The text of value from its first hook; empty without one.
template<class T>
[[nodiscard]]
std::string describe_text(const T& value)
{
    if constexpr (member_describable<T>)
    {
        return std::string{value.describe()};
    }
    else if constexpr (adl::has_describe<T>)
    {
        return std::string{adl::call_describe(value)};
    }
    else if constexpr (ostream_insertable<T>)
    {
        std::ostringstream out;
        out << value;
        return out.str();
    }
    else
    {
        return {};
    }
}

} // namespace easylocal::detail::io
