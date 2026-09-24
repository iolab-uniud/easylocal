#include <easylocal/aggregation.hpp>

#include <compare>
#include <concepts>
#include <iostream>
#include <string_view>

namespace
{

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
    using HierarchicalCost = aggregation::hierarchical_cost<int, long>;

    static_assert(std::three_way_comparable<LexicographicCost>);
    static_assert(std::three_way_comparable<HierarchicalCost>);
    static_assert(!std::same_as<LexicographicCost, HierarchicalCost>);

    constexpr auto lexicographic_a = aggregation::lexicographic{}(1, 100L);
    constexpr auto lexicographic_b = aggregation::lexicographic{}(2, 0L);
    static_assert(lexicographic_a < lexicographic_b);
    static_assert(lexicographic_a.get<0>() == 1);
    static_assert(lexicographic_a.get<1>() == 100L);

    constexpr auto hierarchical_a = aggregation::hierarchical{}(1, 100L);
    constexpr auto hierarchical_b = aggregation::hierarchical{}(1, 101L);
    static_assert(hierarchical_a < hierarchical_b);

    constexpr aggregation::weighted_sum weighted{2, 3};
    static_assert(weighted(4, 5) == 23);

    bool ok = true;

    ok &= expect(
        lexicographic_a < lexicographic_b,
        "lexicographic aggregation compares values in declaration order");
    ok &= expect(
        hierarchical_a < hierarchical_b,
        "hierarchical aggregation compares levels in declaration order");
    ok &= expect(
        weighted(4, 5) == 23,
        "weighted-sum aggregation combines materialized terms");

    constexpr aggregation::weighted_sum mixed_weights{2, 0.5};
    ok &= expect(
        mixed_weights(3, 4.0) == 8.0,
        "weighted-sum aggregation supports heterogeneous arithmetic weights");

    return ok ? 0 : 1;
}
