#pragma once

#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/testing/check.hpp>

#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>

namespace easylocal::testing
{

namespace detail
{

template<class Test, class Instance, class Solution, class NHE>
[[nodiscard]] std::optional<typename NHE::move_type> sample_move(
    const Instance& instance,
    const Solution& solution,
    const NHE& neighborhood)
{
    using move_type = typename NHE::move_type;

    if constexpr (requires { Test::move(instance, solution); })
    {
        return move_type{Test::move(instance, solution)};
    }
    else if constexpr (easylocal::deterministic_neighborhood_for<NHE, Solution>)
    {
        for (auto&& raw_move : easylocal::moves(neighborhood, solution))
        {
            move_type move{raw_move};
            if (static_cast<bool>(neighborhood.is_valid(solution, move)))
            {
                return move;
            }
        }
        return std::nullopt;
    }
    else if constexpr (easylocal::random_neighborhood_for<
                           NHE,
                           Solution,
                           deterministic_rng>)
    {
        deterministic_rng rng;
        for (std::size_t sample = 0;
             sample < random_samples_v<Test>;
             ++sample)
        {
            auto move = easylocal::random_move(neighborhood, solution, rng);
            if (move && static_cast<bool>(neighborhood.is_valid(solution, *move)))
            {
                return move;
            }
        }
        return std::nullopt;
    }
    else
    {
        static_assert(
            requires { Test::move(instance, solution); },
            "delta evaluator check needs a Move; provide a deterministic/random "
            "NeighborhoodExplorer or Test::move(instance, solution)");
        return std::nullopt;
    }
}

template<class Test, class Component, class Value, class Solution, class Move>
[[nodiscard]] Value colocated_updated_value(
    const Component& component,
    const Value& value,
    const Solution& solution,
    const Move& move)
{
    static_assert(
        requires {
            { value + component.delta_evaluate(solution, move) }
                -> std::same_as<Value>;
        },
        "co-located delta evaluation must satisfy Value + "
        "component.delta_evaluate(solution, move) -> Value");

    return value + component.delta_evaluate(solution, move);
}

template<class Test, class Evaluator, class Value, class Solution, class Move>
[[nodiscard]] Value separate_updated_value(
    const Evaluator& evaluator,
    const Value& value,
    const Solution& solution,
    const Move& move)
{
    static_assert(
        requires {
            { value + evaluator.delta_evaluate(solution, move) }
                -> std::same_as<Value>;
        },
        "delta evaluation must satisfy Value + "
        "evaluator.delta_evaluate(solution, move) -> Value");

    return value + evaluator.delta_evaluate(solution, move);
}

} // namespace detail

template<class Test>
[[nodiscard]] check_report check_delta_evaluator()
{
    using neighborhood_type = typename Test::neighborhood;
    using solution_manager_type = detail::test_solution_manager_t<Test>;
    using component_type = typename Test::component;
    using input_type = typename solution_manager_type::input_type;
    using solution_type = typename solution_manager_type::solution_type;

    static_assert(
        easylocal::neighborhood_explorer_for<
            neighborhood_type,
            solution_manager_type>,
        "Test::neighborhood does not satisfy the EasyLocal NeighborhoodExplorer core contract");

    static_assert(
        requires(const component_type& component, const solution_type& solution) {
            component.evaluate(solution);
        },
        "Test::component must provide evaluate(const Solution&) const");

    auto instance = Test::instance();
    static_assert(std::same_as<
        std::remove_cvref_t<decltype(instance)>,
        input_type>);

    auto solution_manager = detail::make_solution_manager<
        Test, input_type, solution_manager_type>(instance);
    auto neighborhood = detail::make_neighborhood<
        Test, solution_manager_type, neighborhood_type>(solution_manager);
    auto component = detail::make_component<
        Test, input_type, component_type>(instance);
    auto solution = Test::solution(instance);
    static_assert(std::same_as<
        std::remove_cvref_t<decltype(solution)>,
        solution_type>);

    check_report report{"delta evaluator"};

    const auto valid_solution = static_cast<bool>(
        solution_manager.is_valid(solution));
    report.check(
        valid_solution,
        "fixture solution",
        "Test::solution(instance) must return a valid Solution");

    if (!valid_solution)
    {
        return report;
    }

    auto move = detail::sample_move<Test>(instance, solution, neighborhood);
    report.check(
        move.has_value(),
        "sample move",
        "the check fixture did not provide or generate a valid Move");

    if (!move)
    {
        return report;
    }

    const auto valid_move = static_cast<bool>(
        neighborhood.is_valid(solution, *move));
    report.check(
        valid_move,
        "sample move",
        "the Move selected for delta checking is not valid for the fixture Solution");

    if (!valid_move)
    {
        return report;
    }

    const auto current = component.evaluate(solution);
    using value_type = std::remove_cvref_t<decltype(current)>;

    const auto incremental = [&]() -> value_type {
        if constexpr (requires { typename Test::delta_evaluator; })
        {
            using delta_evaluator_type = typename Test::delta_evaluator;
            auto evaluator = detail::make_delta_evaluator<
                Test, input_type, delta_evaluator_type>(instance);
            return detail::separate_updated_value<Test>(
                evaluator,
                current,
                solution,
                *move);
        }
        else
        {
            return detail::colocated_updated_value<Test>(
                component,
                current,
                solution,
                *move);
        }
    }();

    auto candidate = solution;
    neighborhood.make_move(candidate, *move);
    report.check(
        static_cast<bool>(solution_manager.is_valid(candidate)),
        "move application",
        "make_move produced an invalid Solution while checking the delta evaluator");

    const auto full = component.evaluate(candidate);
    report.check(
        detail::equivalent<Test>(incremental, full),
        "delta law",
        "incremental evaluation does not match full component evaluation after make_move");

    return report;
}

} // namespace easylocal::testing
