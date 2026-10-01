#pragma once

#include <concepts>
#include <type_traits>

// The cost value contract. Algorithms that need a numeric difference between
// two already-computed costs use cost::delta(candidate, current); operator- is
// convenience syntax only and cost types may provide delta via ADL.
namespace easylocal::cost
{

template<class Cost>
concept arithmetic =
    (std::integral<std::remove_cv_t<Cost>> ||
     std::floating_point<std::remove_cv_t<Cost>>) &&
    (!std::same_as<std::remove_cv_t<Cost>, bool>);

template<arithmetic Cost>
[[nodiscard]]
constexpr auto delta(const Cost& candidate, const Cost& current)
    noexcept(noexcept(candidate - current))
{
    return candidate - current;
}

template<class Cost>
concept has_delta =
    requires(const Cost& candidate, const Cost& current) {
        { delta(candidate, current) } -> std::convertible_to<long double>;
    };

} // namespace easylocal::cost
