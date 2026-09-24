#pragma once

#include <algorithm>
#include <cmath>

namespace easylocal::test_support
{

struct ApproximateTolerance
{
    double relative{};
    double absolute{};
};

[[nodiscard]]
inline auto approximately_equal(
    const double lhs,
    const double rhs,
    const ApproximateTolerance tolerance) noexcept -> bool
{
    if (lhs == rhs)
    {
        return true;
    }

    if (!std::isfinite(lhs) || !std::isfinite(rhs))
    {
        return false;
    }

    const auto difference = std::abs(lhs - rhs);
    const auto scale = std::max(std::abs(lhs), std::abs(rhs));

    return difference <=
           std::max(tolerance.absolute, tolerance.relative * scale);
}

[[nodiscard]]
inline auto definitely_less(
    const double lhs,
    const double rhs,
    const ApproximateTolerance tolerance) noexcept -> bool
{
    return lhs < rhs && !approximately_equal(lhs, rhs, tolerance);
}

} // namespace easylocal::test_support
