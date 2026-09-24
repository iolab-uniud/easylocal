#include "support/approximate.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>

namespace
{

using easylocal::test_support::ApproximateTolerance;
using easylocal::test_support::approximately_equal;
using easylocal::test_support::definitely_less;

auto expect(const bool condition, const std::string_view description) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }

    return true;
}

} // namespace

int main()
{
    bool ok = true;

    const ApproximateTolerance tight{
        .relative = 1.0e-12,
        .absolute = 1.0e-12,
    };

    ok &= expect(
        approximately_equal(1.25, 1.25, tight),
        "exactly equal finite values are approximately equal");
    ok &= expect(
        approximately_equal(0.0, -0.0, tight),
        "signed zeroes are approximately equal");

    ok &= expect(
        approximately_equal(0.0, 5.0e-13, tight),
        "absolute tolerance controls comparisons near zero");
    ok &= expect(
        !approximately_equal(0.0, 2.0e-12, tight),
        "difference beyond absolute tolerance is not hidden near zero");

    ok &= expect(
        approximately_equal(1.0e12, 1.0e12 + 0.5, tight),
        "relative tolerance scales with large finite magnitudes");
    ok &= expect(
        !approximately_equal(1.0e12, 1.0e12 + 2.0, tight),
        "material large-scale difference remains distinguishable");

    const auto one = 1.0;
    const auto next = std::nextafter(one, 2.0);
    ok &= expect(
        approximately_equal(one, next, tight) &&
            approximately_equal(next, one, tight),
        "approximate equality is symmetric for adjacent representable values");

    const auto infinity = std::numeric_limits<double>::infinity();
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    ok &= expect(
        approximately_equal(infinity, infinity, tight),
        "identical infinities preserve exact equality");
    ok &= expect(
        !approximately_equal(infinity, -infinity, tight),
        "opposite infinities are not approximately equal");
    ok &= expect(
        !approximately_equal(infinity, 1.0, tight),
        "finite and infinite values are not approximately equal");
    ok &= expect(
        !approximately_equal(nan, nan, tight),
        "NaN is never approximately equal, including to itself");

    ok &= expect(
        !definitely_less(next, one, tight),
        "larger approximately equal value is not definitely less");
    ok &= expect(
        !definitely_less(std::nextafter(one, 0.0), one, tight),
        "one-ulp numerical decrease is not a definite improvement");
    ok &= expect(
        definitely_less(0.99, 1.0, tight),
        "material decrease remains a definite improvement");

    const ApproximateTolerance absolute_only{
        .relative = 0.0,
        .absolute = 1.0,
    };
    constexpr double a = 0.0;
    constexpr double b = 0.75;
    constexpr double c = 1.5;
    ok &= expect(
        approximately_equal(a, b, absolute_only) &&
            approximately_equal(b, c, absolute_only) &&
            !approximately_equal(a, c, absolute_only),
        "approximate equality is deliberately not assumed transitive");

    return ok ? 0 : 1;
}
