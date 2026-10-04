#pragma once

/// \file
/// limit: the bound of a search on a count (evaluations, iterations), a number
/// or easylocal::unlimited; in text, the number or "unlimited".

#include <cstddef>
#include <limits>

namespace easylocal
{

/// A count that may be unlimited.
///
/// It converts to and from std::size_t, so a limit is set with a number and
/// compared with a count as one; unlimited is the largest std::size_t, which no
/// count reaches. Zero is a limit of zero.
class limit
{
public:
    constexpr limit() noexcept = default;

    // NOLINTNEXTLINE(google-explicit-constructor): a number is a limit
    constexpr limit(const std::size_t count) noexcept : count_{count} {}

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

/// No limit: what a default-constructed limit is.
inline constexpr limit unlimited{};

} // namespace easylocal
