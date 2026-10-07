#include "support/expect.hpp"

#include <easylocal/cost.hpp>
#include <easylocal/helpers/recipes.hpp>

#include <array>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <utility>

namespace
{

template<class Cost>
concept subtractable_cost =
    requires(const Cost& lhs, const Cost& rhs) {
        lhs - rhs;
    };

} // namespace

struct Instance
{
};

struct Solution
{
    int a{1};
    int b{2};
    int c{3};
};

class SolutionManager
    : public easylocal::solution_manager_base<Instance, Solution>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]]
    static auto is_valid(const Solution&) noexcept -> bool
    {
        return true;
    }
};

struct A
{
    [[nodiscard]]
    static auto evaluate(const Solution& solution) noexcept -> int
    {
        return solution.a;
    }
};

struct B
{
    [[nodiscard]]
    static auto evaluate(const Solution& solution) noexcept -> long
    {
        return solution.b;
    }
};

struct Pair
{
    int first{};
    int second{};
};

struct C
{
    [[nodiscard]]
    static auto evaluate(const Solution& solution) noexcept -> Pair
    {
        return {solution.c, -solution.c};
    }
};

struct PairSum
{
    [[nodiscard]]
    auto operator()(const Pair& pair) const noexcept -> int
    {
        return pair.first + 2 * pair.second;
    }
};

template<class Expression>
[[nodiscard]]
auto evaluate(Expression expression)
{
    static const Instance instance{};
    return (easylocal::solution_manager<SolutionManager>() | std::move(expression))
        .construct(instance)
        .evaluate(Solution{});
}

// A hard cost with only < and ==, as delta() accepts it.
struct Violations
{
    int count{0};

    [[nodiscard]]
    friend constexpr bool operator<(const Violations& lhs, const Violations& rhs) noexcept
    {
        return lhs.count < rhs.count;
    }

    [[nodiscard]]
    friend constexpr bool operator==(const Violations&, const Violations&) = default;
};

template<class Expression>
using manager_t = typename decltype(
    easylocal::solution_manager<SolutionManager>()
    | std::declval<Expression>())::service_type;

int main()
{
    namespace cost = easylocal::cost;

    using LexicographicCost = cost::lexicographic<int, long>;
    using HierarchicalCost = cost::hierarchical<LexicographicCost, long>;

    static_assert(std::three_way_comparable<LexicographicCost>);
    static_assert(cost::lexicographic_type<LexicographicCost>);
    static_assert(!cost::lexicographic_type<int>);
    static_assert(cost::arithmetic<int> && cost::arithmetic<double>);
    static_assert(!cost::arithmetic<unsigned> && !cost::arithmetic<std::size_t>);
    static_assert(!cost::arithmetic<unsigned char> && !cost::arithmetic<bool>);
    static_assert(LexicographicCost::levels == 2);
    static_assert(std::three_way_comparable<HierarchicalCost>);
    static_assert(!std::same_as<LexicographicCost, HierarchicalCost>);
    static_assert(!subtractable_cost<LexicographicCost>);
    static_assert(!easylocal::cost::has_delta<LexicographicCost>);
    static_assert(easylocal::cost::has_delta<int>);
    static_assert(easylocal::cost::has_delta<double>);
    static_assert(easylocal::cost::has_delta<cost::hierarchical<int, double>>);
    static_assert(subtractable_cost<cost::hierarchical<int, double>>);

    constexpr auto lexicographic_a = cost::lexicographic{1, 100L};
    constexpr auto lexicographic_b = cost::lexicographic{2, 0L};
    static_assert(lexicographic_a < lexicographic_b);
    static_assert(lexicographic_a.get<0>() == 1);
    static_assert(lexicographic_a.get<1>() == 100L);

    constexpr auto hard_a = cost::lexicographic{1, 100L};
    constexpr auto hard_b = cost::lexicographic{2, 0L};
    constexpr auto hierarchical_a = cost::hierarchical{hard_a, 100L};
    constexpr auto hierarchical_b = cost::hierarchical{hard_a, 101L};
    constexpr auto hierarchical_c = cost::hierarchical{hard_b, 0L};
    static_assert(hierarchical_a < hierarchical_b);
    static_assert(hierarchical_b < hierarchical_c);
    static_assert(hierarchical_a.hard() == hard_a);
    static_assert(hierarchical_a.soft() == 100L);

    // A hard cost with only < and == compares too, as delta() takes it.
    using ViolationsCost = cost::hierarchical<Violations, double>;
    static_assert(std::three_way_comparable<ViolationsCost>);
    static_assert(easylocal::cost::has_delta<ViolationsCost>);
    static_assert(
        ViolationsCost{Violations{0}, 5.0} < ViolationsCost{Violations{1}, 0.0});
    static_assert(
        ViolationsCost{Violations{1}, 0.0} < ViolationsCost{Violations{1}, 2.0});
    static_assert(
        ViolationsCost{Violations{1}, 2.0} == ViolationsCost{Violations{1}, 2.0});
    static_assert(
        ViolationsCost{Violations{1}, 2.0} >= ViolationsCost{Violations{0}, 9.0});

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

    const auto numeric_a = cost::hierarchical{0, 10.0};
    const auto numeric_b = cost::hierarchical{0, 13.5};
    const auto hard_better = cost::hierarchical{0, 1000.0};
    const auto hard_worse = cost::hierarchical{1, -1000.0};
    ok &= expect(
        easylocal::cost::delta(13.5, 10.0) == 3.5,
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

    using easylocal::component;

    // A single component: its value is the cost.
    ok &= expect(evaluate(component<A>()) == 1, "a component is a cost");

    // sum: unit weights unless given, computed in the common type of the
    // costs and the weights.
    const auto unit_sum = evaluate(cost::sum(component<A>(), component<B>()));
    static_assert(std::same_as<decltype(unit_sum), const long>);
    ok &= expect(unit_sum == 3, "a sum has unit weights by default");

    const auto weighted_sum = evaluate(cost::sum(
        cost::weighted(component<A>(), 0.5),
        cost::weighted(component<B>(), 10)));
    static_assert(std::same_as<decltype(weighted_sum), const double>);
    ok &= expect(weighted_sum == 20.5, "a sum weighs its terms");

    // child * w and w * child are shorthands for cost::weighted(child, w).
    static_assert(std::same_as<
        decltype(component<A>() * 2),
        decltype(cost::weighted(component<A>(), 2))>);
    static_assert(std::same_as<
        decltype(0.5 * component<A>()),
        decltype(cost::weighted(component<A>(), 0.5))>);
    static_assert(std::same_as<
        decltype(cost::in_order(component<A>()) * 3),
        decltype(cost::weighted(cost::in_order(component<A>()), 3))>);
    ok &= expect(
        evaluate(cost::sum(0.5 * component<A>(), component<B>() * 10)) == 20.5,
        "the * shorthand weighs the terms of a sum");

    // in_order: a lexicographic cost of the children's costs.
    const auto ordered = evaluate(cost::in_order(
        component<B>(),
        cost::sum(cost::weighted(component<A>(), 3))));
    static_assert(std::same_as<
        std::remove_cvref_t<decltype(ordered)>,
        cost::lexicographic<long, int>>);
    ok &= expect(
        ordered.get<0>() == 2 && ordered.get<1>() == 3,
        "in_order compares its children in order");

    // apply: a function of the children's costs, for domain values too.
    ok &= expect(
        evaluate(cost::apply(PairSum{}, component<C>())) == -3,
        "apply maps domain values to a cost");
    ok &= expect(
        evaluate(cost::sum(
            cost::weighted(cost::apply(PairSum{}, component<C>()), 2),
            component<A>())) == -5,
        "apply nests inside a sum");

    // hard_soft: a hierarchical cost whose hard components come first.
    using Hierarchical = decltype(cost::hard_soft(
        cost::in_order(component<A>(), component<B>()),
        cost::apply(PairSum{}, component<C>())));
    static_assert(manager_t<Hierarchical>::has_hard_component_projection);
    static_assert(manager_t<Hierarchical>::hard_component_count == 2);
    static_assert(!manager_t<decltype(cost::in_order(component<A>()))>::
                      has_hard_component_projection);

    const auto hierarchical = evaluate(cost::hard_soft(
        cost::in_order(component<A>(), component<B>()),
        cost::apply(PairSum{}, component<C>())));
    ok &= expect(
        hierarchical.hard().get<0>() == 1 &&
            hierarchical.hard().get<1>() == 2 &&
            hierarchical.soft() == -3,
        "hard_soft builds the hierarchical cost");

    return ok ? 0 : 1;
}
