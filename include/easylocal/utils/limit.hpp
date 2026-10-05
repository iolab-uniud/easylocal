#pragma once

/// \file
/// limit: the bound of a search on a count (evaluations, iterations), a number
/// or easylocal::unlimited (a tag of type unlimited_t); in text, the number or
/// "unlimited".

#include <cstddef>
#include <limits>

namespace easylocal
{

/// The type of easylocal::unlimited: no limit for a count, and any value as
/// the domain of a parameter.
///
/// It converts to a limit, the unlimited one; a number never converts to it,
/// so `range(1, 1000)` and `range(1, unlimited)` cannot be confused.
struct unlimited_t
{
    /// The tag; easylocal::unlimited is the one to use.
    explicit constexpr unlimited_t() noexcept = default;
};

/// A count that may be unlimited.
///
/// It converts to and from std::size_t, so a limit is set with a number and
/// compared with a count as one; unlimited is the largest std::size_t, which no
/// count reaches. Zero is a limit of zero.
class limit
{
public:
    /// No limit: unlimited.
    constexpr limit() noexcept = default;

    /// A limit of count.
    // NOLINTNEXTLINE(google-explicit-constructor): a number is a limit
    constexpr limit(const std::size_t count) noexcept : count_{count} {}

    /// No limit.
    // NOLINTNEXTLINE(google-explicit-constructor): unlimited is a limit
    constexpr limit(unlimited_t) noexcept {}

    /// The count: unlimited is the largest std::size_t.
    // NOLINTNEXTLINE(google-explicit-constructor): a limit is compared as a count
    [[nodiscard]]
    constexpr operator std::size_t() const noexcept
    {
        return count_;
    }

    /// Whether the limit is unlimited.
    [[nodiscard]]
    constexpr bool is_unlimited() const noexcept
    {
        return count_ == std::numeric_limits<std::size_t>::max();
    }

private:
    std::size_t count_{std::numeric_limits<std::size_t>::max()};
};

/// No limit, what a default-constructed limit is, for a count; any value, as
/// the domain of a parameter.
inline constexpr unlimited_t unlimited{};

} // namespace easylocal
