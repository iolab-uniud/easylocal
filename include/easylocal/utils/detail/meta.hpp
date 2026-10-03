#pragma once

#include <concepts>
#include <cstddef>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

// Internal type-level utilities shared across EasyLocal modules.
namespace easylocal::detail
{

template<class>
inline constexpr bool always_false_v = false;

template<class T, class... Ts>
inline constexpr bool type_in_pack_v = (std::same_as<T, Ts> || ...);

template<class... Ts>
struct unique_types : std::true_type
{
};

template<class T, class... Rest>
struct unique_types<T, Rest...>
    : std::bool_constant<
          !type_in_pack_v<T, Rest...> && unique_types<Rest...>::value>
{
};

template<class... Ts>
inline constexpr bool unique_types_v = unique_types<Ts...>::value;

template<class T, class Tuple>
struct tuple_contains_type;

template<class T, class... Ts>
struct tuple_contains_type<T, std::tuple<Ts...>>
    : std::bool_constant<type_in_pack_v<T, Ts...>>
{
};

template<class T, class Tuple>
inline constexpr bool tuple_contains_type_v = tuple_contains_type<T, Tuple>::value;

template<class T, class Tuple>
struct tuple_type_index;

template<class T, class... Rest>
struct tuple_type_index<T, std::tuple<T, Rest...>>
    : std::integral_constant<std::size_t, 0>
{
};

template<class T, class First, class... Rest>
struct tuple_type_index<T, std::tuple<First, Rest...>>
    : std::integral_constant<
          std::size_t,
          1 + tuple_type_index<T, std::tuple<Rest...>>::value>
{
};

template<class T, class Tuple>
inline constexpr std::size_t tuple_type_index_v = tuple_type_index<T, Tuple>::value;

template<class Tuple, std::size_t... Indices>
[[nodiscard]]
std::tuple<std::tuple_element_t<Indices, Tuple>...>
    tuple_prefix_type_impl(std::index_sequence<Indices...>);

template<class Tuple, std::size_t Count>
using tuple_prefix_t = decltype(
    tuple_prefix_type_impl<Tuple>(std::make_index_sequence<Count>{}));

template<class T, class = void>
struct tuple_size_or_zero : std::integral_constant<std::size_t, 0>
{
};

template<class T>
struct tuple_size_or_zero<T, std::void_t<decltype(std::tuple_size<T>::value)>>
    : std::integral_constant<std::size_t, std::tuple_size_v<T>>
{
};

template<class T>
inline constexpr std::size_t tuple_size_or_zero_v = tuple_size_or_zero<T>::value;

template<class T>
struct is_variant : std::false_type
{
};

template<class... Ts>
struct is_variant<std::variant<Ts...>> : std::true_type
{
};

template<class T>
inline constexpr bool is_variant_v = is_variant<std::remove_cvref_t<T>>::value;

template<class T>
struct optional_value;

template<class T>
struct optional_value<std::optional<T>>
{
    using type = T;
};

template<class T>
using optional_value_t = typename optional_value<std::remove_cvref_t<T>>::type;

} // namespace easylocal::detail
