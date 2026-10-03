#pragma once

// Parameter blocks: plain structs that describe their own fields with a
// compile-time schema (config::field, config::group, config::fields) and check
// them with validate(). Runners, neighborhoods, cost expressions and programs
// declare their parameters this way.

#include <concepts>
#include <cstddef>
#include <functional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::config
{

template<std::size_t Size>
struct fixed_string
{
    char value[Size]{};

    consteval fixed_string(const char (&text)[Size])
    {
        for (std::size_t index = 0; index < Size; ++index)
        {
            value[index] = text[index];
        }
    }

    [[nodiscard]]
    constexpr std::string_view view() const noexcept
    {
        static_assert(Size >= 1);
        return {value, Size - 1};
    }
};

template<std::size_t Size>
fixed_string(const char (&)[Size]) -> fixed_string<Size>;

struct validation_result
{
    bool valid{true};
    std::string_view message{};

    [[nodiscard]]
    constexpr explicit operator bool() const noexcept
    {
        return valid;
    }

    [[nodiscard]]
    static constexpr validation_result success() noexcept
    {
        return {};
    }

    [[nodiscard]]
    static constexpr validation_result failure(const std::string_view message) noexcept
    {
        return {.valid = false, .message = message};
    }
};

namespace detail
{

template<class MemberPointer>
struct member_pointer_traits;

template<class Value, class Owner>
struct member_pointer_traits<Value Owner::*>
{
    using owner_type = Owner;
    using value_type = Value;
};

template<class... Fields>
[[nodiscard]]
consteval bool unique_field_names() noexcept
{
    if constexpr (sizeof...(Fields) <= 1)
    {
        return true;
    }
    else
    {
        return []<class First, class... Rest>(std::type_identity<First>,
                                              std::type_identity<Rest>...) {
            return ((First::name() != Rest::name()) && ...) &&
                   unique_field_names<Rest...>();
        }(std::type_identity<Fields>{}...);
    }
}

} // namespace detail

template<fixed_string Name, auto Member>
    requires std::is_member_object_pointer_v<decltype(Member)>
struct parameter_field
{
    static_assert(
        !Name.view().empty(),
        "parameter field name must not be empty");

    using member_pointer_type = decltype(Member);
    using owner_type = typename detail::member_pointer_traits<
        member_pointer_type>::owner_type;
    using value_type = typename detail::member_pointer_traits<
        member_pointer_type>::value_type;

    static constexpr auto member = Member;

    std::string_view description{};

    [[nodiscard]]
    static constexpr std::string_view name() noexcept
    {
        return Name.view();
    }
};

template<fixed_string Name, auto Member>
    requires std::is_member_object_pointer_v<decltype(Member)>
[[nodiscard]]
constexpr parameter_field<Name, Member> field(
    const std::string_view description = {}) noexcept
{
    return {.description = description};
}

// A member that is itself a parameter block, nested in the schema: its fields
// are under "Name.", and its validate() runs with the enclosing block's, e.g.
// group<"temperature", &AnnealingParameters::temperature>("Temperature schedule").
template<fixed_string Name, auto Member>
    requires std::is_member_object_pointer_v<decltype(Member)>
struct parameter_group
{
    static_assert(!Name.view().empty(), "parameter group name must not be empty");

    using member_pointer_type = decltype(Member);
    using owner_type =
        typename detail::member_pointer_traits<member_pointer_type>::owner_type;
    using value_type =
        typename detail::member_pointer_traits<member_pointer_type>::value_type;

    static constexpr auto member = Member;

    std::string_view description{};

    [[nodiscard]]
    static constexpr std::string_view name() noexcept
    {
        return Name.view();
    }
};

template<fixed_string Name, auto Member>
    requires std::is_member_object_pointer_v<decltype(Member)>
[[nodiscard]]
constexpr parameter_group<Name, Member> group(
    const std::string_view description = {}) noexcept
{
    return {.description = description};
}

template<class Descriptor>
inline constexpr bool is_parameter_group_v = false;

template<fixed_string Name, auto Member>
inline constexpr bool is_parameter_group_v<parameter_group<Name, Member>> = true;

template<class... Fields>
[[nodiscard]]
constexpr std::tuple<Fields...> fields(Fields... parameter_fields) noexcept
{
    static_assert(
        detail::unique_field_names<Fields...>(),
        "parameter fields must have unique names within a block");
    return {std::move(parameter_fields)...};
}

template<class T>
concept parameter_block =
    requires(const T& parameters)
    {
        { T::parameter_schema() };
        { parameters.validate() } -> std::same_as<validation_result>;
    };

template<class T>
using configurable_parameters_t = std::remove_cvref_t<decltype(
    std::declval<const std::remove_cvref_t<T>&>().parameters())>;

template<class T>
concept configurable_endpoint =
    requires(
        std::remove_cvref_t<T>& endpoint,
        const std::remove_cvref_t<T>& const_endpoint,
        configurable_parameters_t<T> parameters)
    {
        requires parameter_block<configurable_parameters_t<T>>;
        {
            const_endpoint.parameters()
        } -> std::same_as<const configurable_parameters_t<T>&>;
        {
            endpoint.configure(std::move(parameters))
        } -> std::same_as<validation_result>;
    };

// The fields of a block, without its nested groups: function(descriptor, value).
template<parameter_block Parameters, class Function>
constexpr void for_each_parameter(
    Parameters& parameters,
    Function&& function)
{
    auto schema = Parameters::parameter_schema();

    std::apply(
        [&parameters, &function](auto... descriptors) {
            (
                [&] {
                    using descriptor_type = decltype(descriptors);
                    static_assert(std::same_as<
                        std::remove_cv_t<typename descriptor_type::owner_type>,
                        std::remove_cv_t<Parameters>>);
                    if constexpr (!is_parameter_group_v<descriptor_type>)
                        std::invoke(
                            function,
                            descriptors,
                            parameters.*descriptor_type::member);
                }(),
                ...);
        },
        std::move(schema));
}

template<parameter_block Parameters, class Function>
constexpr void for_each_parameter(
    const Parameters& parameters,
    Function&& function)
{
    auto schema = Parameters::parameter_schema();

    std::apply(
        [&parameters, &function](auto... descriptors) {
            (
                [&] {
                    using descriptor_type = decltype(descriptors);
                    static_assert(std::same_as<
                        std::remove_cv_t<typename descriptor_type::owner_type>,
                        std::remove_cv_t<Parameters>>);
                    if constexpr (!is_parameter_group_v<descriptor_type>)
                        std::invoke(
                            function,
                            descriptors,
                            parameters.*descriptor_type::member);
                }(),
                ...);
        },
        std::move(schema));
}

} // namespace easylocal::config
