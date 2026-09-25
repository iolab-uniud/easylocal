#pragma once

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
    constexpr auto view() const noexcept -> std::string_view
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
    static constexpr auto success() noexcept -> validation_result
    {
        return {};
    }

    [[nodiscard]]
    static constexpr auto failure(const std::string_view message) noexcept
        -> validation_result
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

} // namespace detail

template<fixed_string Name, auto Member>
    requires std::is_member_object_pointer_v<decltype(Member)>
struct parameter_field
{
    using member_pointer_type = decltype(Member);
    using owner_type = typename detail::member_pointer_traits<
        member_pointer_type>::owner_type;
    using value_type = typename detail::member_pointer_traits<
        member_pointer_type>::value_type;

    static constexpr auto member = Member;

    std::string_view description{};

    [[nodiscard]]
    static constexpr auto name() noexcept -> std::string_view
    {
        return Name.view();
    }
};

template<fixed_string Name, auto Member>
    requires std::is_member_object_pointer_v<decltype(Member)>
[[nodiscard]]
constexpr auto field(const std::string_view description = {}) noexcept
    -> parameter_field<Name, Member>
{
    return {.description = description};
}

template<class... Fields>
[[nodiscard]]
constexpr auto fields(Fields... parameter_fields) noexcept
    -> std::tuple<Fields...>
{
    return {std::move(parameter_fields)...};
}

template<class T>
concept parameter_block =
    requires(const T& parameters)
    {
        { T::parameter_schema() };
        { parameters.validate() } -> std::same_as<validation_result>;
    };

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
