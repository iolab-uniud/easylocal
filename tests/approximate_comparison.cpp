// cost::tolerance and the approximate comparisons of costs.
#include "support/expect.hpp"

#include <easylocal/cost.hpp>

#include <cmath>
#include <compare>
#include <limits>

namespace
{

using easylocal::cost::approximate_compare;
using easylocal::cost::approximately_equal;
using ApproximateTolerance = easylocal::cost::tolerance;

[[nodiscard]]
auto definitely_less(
    const double lhs,
    const double rhs,
    const ApproximateTolerance tolerance) -> bool
{
    return approximate_compare(lhs, rhs, tolerance) < 0;
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

    ok &= expect(
        approximately_equal(3, 3, tight) && !approximately_equal(3, 4, absolute_only)
            && approximately_equal(3, 3.0 + 1.0e-13, tight),
        "integers compare exactly, a floating-point value with the tolerance");

    using easylocal::cost::hierarchical;
    using easylocal::cost::lexicographic;
    using easylocal::cost::pareto;
    const hierarchical<int, double> drifted{0, 0.1 + 0.2};
    ok &= expect(
        approximately_equal(drifted, hierarchical<int, double>{0, 0.3}, tight)
            && !approximately_equal(drifted, hierarchical<int, double>{1, 0.3}, tight)
            && approximate_compare(drifted, hierarchical<int, double>{0, 0.3}, tight) == 0
            && approximate_compare(drifted, hierarchical<int, double>{0, 0.4}, tight) < 0,
        "a hierarchical cost compares its hard, then its soft cost, within the tolerance");
    ok &= expect(
        approximate_compare(
            lexicographic<double, int>{0.1 + 0.2, 5},
            lexicographic<double, int>{0.3, 4},
            tight)
            > 0,
        "a lexicographic level equal within the tolerance passes to the next");
    ok &= expect(
        approximate_compare(
            pareto<double, int>{0.1 + 0.2, 1},
            pareto<double, int>{0.3, 2},
            tight)
                < 0
            && approximate_compare(
                   pareto<double, int>{0.1 + 0.2, 3},
                   pareto<double, int>{0.2, 2},
                   tight)
                > 0
            && approximate_compare(
                   pareto<double, int>{0.1, 3},
                   pareto<double, int>{0.2, 2},
                   tight)
                == std::partial_ordering::unordered,
        "a pareto cost dominates by objectives compared within the tolerance");
    ok &= expect(
        approximate_compare(nan, 1.0, tight) == std::partial_ordering::unordered,
        "NaN is unordered");
    ok &= expect(
        ApproximateTolerance{}(1.0, 1.0 + 1.0e-12)
            && !ApproximateTolerance{}(1.0, 1.0001),
        "a tolerance compares two values when called");

    return ok ? 0 : 1;
}
