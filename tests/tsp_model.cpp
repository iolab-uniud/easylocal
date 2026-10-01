#include "instance.hpp"
#include "solution.hpp"
#include "solution_manager.hpp"
#include "tour_length_component.hpp"

#include <easylocal/cost.hpp>
#include <easylocal/runners/runner.hpp>

#include <compare>
#include <concepts>
#include <iostream>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

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
    using namespace easylocal::mwe::tsp;
    using easylocal::aggregator;
    using easylocal::component;
    using easylocal::solution_manager;

    bool ok = true;

    const TspInstance instance{
        .city_count = 5,
        .distances = {
            0.0, 1.0, 2.0, 2.5, 1.5,
            1.0, 0.0, 1.25, 2.0, 2.5,
            2.0, 1.25, 0.0, 1.0, 2.0,
            2.5, 2.0, 1.0, 0.0, 1.25,
            1.5, 2.5, 2.0, 1.25, 0.0,
        },
    };

    const auto manager_recipe =
        solution_manager<TspSolutionManager>()
        | component<TourLengthComponent>()
        | aggregator(TourLengthCost{});
    const auto manager = manager_recipe.construct(instance);

    static_assert(std::same_as<typename decltype(manager)::cost_type, double>);
    static_assert(std::three_way_comparable<typename decltype(manager)::cost_type>);
    static_assert(std::same_as<
                  decltype(distance_type{} <=> distance_type{}),
                  std::partial_ordering>);

    const Tour initial{
        .tour = {0, 2, 1, 3, 4},
    };

    ok &= expect(
        manager.is_valid(initial),
        "a permutation containing every city exactly once is valid");

    const auto component_values = manager.evaluate_components(initial);
    ok &= expect(
        std::get<0>(component_values) == TourLengthValue{8.0},
        "tour length is materialized as a structured component value");
    ok &= expect(
        manager.cost_from_components(component_values) == 8.0,
        "structured tour length aggregates to scalar double cost");
    ok &= expect(
        manager.evaluate(initial) == 8.0,
        "configured manager evaluates the complete tour cost");

    Tour copy = initial;
    copy.tour[1] = 1;

    ok &= expect(
        initial.tour == std::vector<city_id>{0, 2, 1, 3, 4},
        "solution retains ordinary value semantics");

    const Tour wrong_size{
        .tour = {0, 1, 2, 3},
    };
    ok &= expect(
        !manager.is_valid(wrong_size),
        "tour with the wrong cardinality is invalid");

    const Tour duplicate_city{
        .tour = {0, 1, 2, 2, 4},
    };
    ok &= expect(
        !manager.is_valid(duplicate_city),
        "tour containing a duplicate city is invalid");

    const Tour out_of_range_city{
        .tour = {0, 1, 2, 3, 5},
    };
    ok &= expect(
        !manager.is_valid(out_of_range_city),
        "tour containing an out-of-range city is invalid");

    return ok ? 0 : 1;
}
