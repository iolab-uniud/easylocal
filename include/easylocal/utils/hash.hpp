#pragma once

/// \file
/// Helpers to write the hash of a solution: hash_combine folds the hash of a
/// value into a running hash, hash_range folds every element of a range. The
/// result depends on the order of the values, and on std::hash, so it may
/// differ between standard libraries.

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ranges>

namespace easylocal
{

template<class T>
concept std_hashable = requires(const T& value) {
    { std::hash<T>{}(value) } -> std::convertible_to<std::size_t>;
};

namespace detail
{

// The SplitMix64 finalizer: spreads every input bit over the result, so that
// identity hashes (std::hash<int> in libstdc++) still combine well.
[[nodiscard]]
constexpr std::uint64_t mix_hash(std::uint64_t value) noexcept
{
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

} // namespace detail

template<std_hashable T>
[[nodiscard]]
constexpr std::uint64_t hash_combine(const std::uint64_t seed, const T& value) noexcept(
    noexcept(std::hash<T>{}(value)))
{
    return detail::mix_hash(
        seed + 0x9e3779b97f4a7c15ULL + static_cast<std::uint64_t>(std::hash<T>{}(value)));
}

template<std::ranges::input_range Range>
    requires std_hashable<std::ranges::range_value_t<Range>>
[[nodiscard]]
constexpr std::uint64_t hash_range(Range&& range, std::uint64_t seed = 0)
{
    for (const auto& value : range)
        seed = hash_combine(seed, value);
    return seed;
}

} // namespace easylocal
