#pragma once

#include <concepts>
#include <type_traits>

namespace easylocal
{

template<class Cost>
concept arithmetic_cost =
    (std::integral<std::remove_cv_t<Cost>> ||
     std::floating_point<std::remove_cv_t<Cost>>) &&
    (!std::same_as<std::remove_cv_t<Cost>, bool>);

template<arithmetic_cost Cost>
[[nodiscard]]
constexpr auto delta(const Cost& candidate, const Cost& current)
    noexcept(noexcept(candidate - current))
{
    return candidate - current;
}

template<class Cost>
concept delta_cost =
    requires(const Cost& candidate, const Cost& current) {
        { delta(candidate, current) } -> std::convertible_to<long double>;
    };

} // namespace easylocal
