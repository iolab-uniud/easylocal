#include <easylocal/aggregation.hpp>

#include <array>
#include <compare>
#include <concepts>
#include <cmath>
#include <iostream>
#include <string_view>

namespace
{

template<class Cost>
concept subtractable_cost =
    requires(const Cost& lhs, const Cost& rhs) {
        lhs - rhs;
    };

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
    namespace aggregation = easylocal::aggregation;

    using LexicographicCost = aggregation::lexicographic_cost<int, long>;
    using HierarchicalCost = aggregation::hierarchical_cost<LexicographicCost, long>;

    static_assert(std::three_way_comparable<LexicographicCost>);
    static_assert(std::three_way_comparable<HierarchicalCost>);
    static_assert(!std::same_as<LexicographicCost, HierarchicalCost>);
    static_assert(!subtractable_cost<LexicographicCost>);
    static_assert(!easylocal::delta_cost<LexicographicCost>);
    static_assert(easylocal::delta_cost<int>);
    static_assert(easylocal::delta_cost<double>);
    static_assert(easylocal::delta_cost<aggregation::hierarchical_cost<int, double>>);
    static_assert(subtractable_cost<aggregation::hierarchical_cost<int, double>>);

    constexpr auto lexicographic_a = aggregation::lexicographic{}(1, 100L);
    constexpr auto lexicographic_b = aggregation::lexicographic{}(2, 0L);
    static_assert(lexicographic_a < lexicographic_b);
    static_assert(lexicographic_a.get<0>() == 1);
    static_assert(lexicographic_a.get<1>() == 100L);

    constexpr auto hard_a = aggregation::lexicographic{}(1, 100L);
    constexpr auto hard_b = aggregation::lexicographic{}(2, 0L);
    constexpr auto hierarchical_a = aggregation::hierarchical{}(hard_a, 100L);
    constexpr auto hierarchical_b = aggregation::hierarchical{}(hard_a, 101L);
    constexpr auto hierarchical_c = aggregation::hierarchical{}(hard_b, 0L);
    static_assert(hierarchical_a < hierarchical_b);
    static_assert(hierarchical_b < hierarchical_c);
    static_assert(hierarchical_a.hard() == hard_a);
    static_assert(hierarchical_a.soft() == 100L);

    constexpr aggregation::weighted_sum weighted{2, 3};
    static_assert(weighted(4, 5) == 23);

    bool ok = true;

    ok &= expect(
        lexicographic_a < lexicographic_b,
        "lexicographic aggregation compares values in declaration order");
    ok &= expect(
        hierarchical_a < hierarchical_b,
        "hierarchical aggregation compares soft cost after equivalent hard cost");
    ok &= expect(
        hierarchical_b < hierarchical_c,
        "hierarchical aggregation gives hard cost strict priority over soft cost");

    const auto numeric_a = aggregation::hierarchical{}(0, 10.0);
    const auto numeric_b = aggregation::hierarchical{}(0, 13.5);
    const auto hard_better = aggregation::hierarchical{}(0, 1000.0);
    const auto hard_worse = aggregation::hierarchical{}(1, -1000.0);
    ok &= expect(
        easylocal::delta(13.5, 10.0) == 3.5,
        "numeric delta is the ordinary arithmetic difference");
    ok &= expect(
        delta(numeric_b, numeric_a) == 3.5L &&
            numeric_b - numeric_a == delta(numeric_b, numeric_a),
        "hierarchical operator- is syntactic sugar for delta");
    ok &= expect(
        std::isinf(hard_better - hard_worse) &&
            (hard_better - hard_worse) < 0.0L,
        "hierarchical delta maps hard improvements to negative infinity");
    ok &= expect(
        std::isinf(hard_worse - hard_better) &&
            (hard_worse - hard_better) > 0.0L,
        "hierarchical delta maps hard worsening to positive infinity");
    ok &= expect(
        weighted(4, 5) == 23,
        "weighted-sum aggregation combines materialized terms");

    constexpr aggregation::weighted_sum_with_hard_penalty penalized{
        1000,
        std::array{10, 1}};
    static_assert(penalized(2, 3, 4) == 2034);

    constexpr aggregation::weighted_sum mixed_weights{2, 0.5};
    ok &= expect(
        mixed_weights(3, 4.0) == 8.0,
        "weighted-sum aggregation supports heterogeneous arithmetic weights");

    return ok ? 0 : 1;
}
