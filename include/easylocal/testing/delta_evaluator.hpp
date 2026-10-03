#pragma once

// check_delta_evaluator: for the moves of the fixture Solution, value + delta
// equals the component's value after the move, for separate and co-located
// delta evaluators.

#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/testing/check.hpp>
#include <easylocal/testing/fixture.hpp>

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace easylocal::testing
{

namespace detail
{

// Tags the co-located case: the component computes its own delta.
struct colocated_delta
{
};

template<class Fixture, class NHE, class Component, class Delta>
[[nodiscard]] check_report check_delta_law(
    const Fixture& fixture,
    const NHE& neighborhood,
    const Component& component,
    const Delta& delta)
{
    using solution_manager_type = typename Fixture::solution_manager_type;
    using solution_type = typename Fixture::solution_type;
    using move_type = typename NHE::move_type;

    static_assert(
        neighborhood_explorer_for<NHE, solution_manager_type>,
        "the NeighborhoodExplorer does not satisfy the EasyLocal NeighborhoodExplorer "
        "core contract for the fixture's SolutionManager");
    static_assert(
        requires(const Component& c, const solution_type& solution) {
            c.evaluate(solution);
        },
        "the cost component must provide evaluate(const Solution&) const");

    const auto& solution_manager = fixture.solution_manager();
    const auto& solution = fixture.solution();

    check_report report{"delta evaluator"};

    if (!check_fixture_solution(report, fixture))
    {
        return report;
    }

    const auto current = component.evaluate(solution);
    using value_type = std::remove_cvref_t<decltype(current)>;

    const auto delta_of = [&](const move_type& move) {
        if constexpr (std::same_as<Delta, colocated_delta>)
        {
            static_assert(
                requires {
                    {
                        current + component.delta_evaluate(solution, move)
                    } -> std::same_as<value_type>;
                },
                "co-located delta evaluation must satisfy Value + "
                "component.delta_evaluate(solution, move) -> Value");
            return component.delta_evaluate(solution, move);
        }
        else
        {
            static_assert(
                requires {
                    {
                        current + delta.delta_evaluate(solution, move)
                    } -> std::same_as<value_type>;
                },
                "delta evaluation must satisfy Value + "
                "evaluator.delta_evaluate(solution, move) -> Value");
            return delta.delta_evaluate(solution, move);
        }
    };

    std::size_t checked = 0;
    const auto check_move = [&](const move_type& move) {
        if (!static_cast<bool>(neighborhood.is_valid(solution, move)))
            return;
        ++checked;

        const value_type incremental = current + delta_of(move);

        auto candidate = solution;
        neighborhood.make_move(candidate, move);
        report.check(
            static_cast<bool>(solution_manager.is_valid(candidate)),
            "move application",
            "make_move produced an invalid Solution while checking the delta");

        report.check(
            fixture.equivalent(incremental, component.evaluate(candidate)),
            "delta law",
            "incremental evaluation does not match full component evaluation after make_move");
    };

    if constexpr (deterministic_neighborhood_for<NHE, solution_type>)
    {
        std::size_t seen = 0;
        for (auto&& raw_move : easylocal::moves(neighborhood, solution))
        {
            if (seen++ == fixture.options().max_enumerated_moves)
            {
                break;
            }
            check_move(move_type{raw_move});
        }
    }
    else
    {
        static_assert(
            random_neighborhood_for<NHE, solution_type, deterministic_rng>,
            "the delta check needs moves: the NeighborhoodExplorer must enumerate "
            "or sample them");
        deterministic_rng rng;
        for (std::size_t sample = 0; sample < fixture.options().random_samples; ++sample)
        {
            if (auto move = easylocal::random_move(neighborhood, solution, rng))
            {
                check_move(*move);
            }
        }
    }

    report.check(
        checked != 0,
        "sample move",
        "the NeighborhoodExplorer produced no valid Move for the fixture Solution");

    return report;
}

} // namespace detail

// For the valid moves of the fixture Solution (enumerated, or sampled when the
// neighborhood cannot enumerate), value + delta equals the component's value
// after the move. The delta comes from a separate delta evaluator...
template<check_fixture Fixture, class NHE, class Component, class DeltaEvaluator>
[[nodiscard]] check_report check_delta_evaluator(
    const Fixture& fixture,
    const NHE& neighborhood,
    const Component& component,
    const DeltaEvaluator& delta_evaluator)
{
    return detail::check_delta_law(fixture, neighborhood, component, delta_evaluator);
}

// ...or from the component's own delta_evaluate (co-located).
template<check_fixture Fixture, class NHE, class Component>
[[nodiscard]] check_report check_delta_evaluator(
    const Fixture& fixture,
    const NHE& neighborhood,
    const Component& component)
{
    return detail::check_delta_law(
        fixture,
        neighborhood,
        component,
        detail::colocated_delta{});
}

// The same, with every object built from the fixture; omit DeltaEvaluator for
// a co-located delta.
template<class NHE, class Component, class DeltaEvaluator = void, check_fixture Fixture>
[[nodiscard]] check_report check_delta_evaluator(const Fixture& fixture)
{
    const auto neighborhood = detail::make_neighborhood<NHE>(fixture.solution_manager());
    const auto component = detail::make_from_input<Component>(fixture.input());
    if constexpr (std::is_void_v<DeltaEvaluator>)
    {
        return check_delta_evaluator(fixture, neighborhood, component);
    }
    else
    {
        return check_delta_evaluator(
            fixture,
            neighborhood,
            component,
            detail::make_from_input<DeltaEvaluator>(fixture.input()));
    }
}

} // namespace easylocal::testing
