#pragma once

#include <compare>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

// Lexicographically ordered cost: values are compared in declaration order.
// A lexicographic cost has no numeric delta.
namespace easylocal::cost
{

template<class... Values>
class lexicographic
{
public:
    constexpr explicit lexicographic(Values... values)
        : values_{std::move(values)...}
    {
    }

    template<std::size_t Index>
    [[nodiscard]]
    constexpr auto get() const noexcept
        -> const std::tuple_element_t<Index, std::tuple<Values...>>&
    {
        return std::get<Index>(values_);
    }

    auto operator<=>(const lexicographic&) const = default;

private:
    std::tuple<Values...> values_;
};

template<class T>
struct lexicographic_traits
{
    static constexpr bool value = false;
    static constexpr std::size_t size = 0;
};

template<class... Values>
struct lexicographic_traits<lexicographic<Values...>>
{
    static constexpr bool value = true;
    static constexpr std::size_t size = sizeof...(Values);
};

template<class T>
inline constexpr bool is_lexicographic_v =
    lexicographic_traits<std::remove_cvref_t<T>>::value;

template<class T>
concept lexicographic_type = is_lexicographic_v<T>;

} // namespace easylocal::cost
