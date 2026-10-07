#pragma once

/// \file
/// check_delta_cost_component: for the moves of the fixture Solution and of
/// random solutions, value + delta equals the component's value after the
/// move, for separate and co-located delta cost components.

#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/testing/check.hpp>
#include <easylocal/testing/detail/support.hpp>
#include <easylocal/testing/fixture.hpp>

#include <concepts>
#include <cstddef>
#include <optional>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

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

    check_report report{"delta cost component"};

    if (!check_fixture_solution(report, fixture))
        return report;

    using value_type =
        std::remove_cvref_t<decltype(component.evaluate(fixture.solution()))>;

    const auto delta_of = [&](const solution_type& solution, const move_type& move) {
        if constexpr (std::same_as<Delta, colocated_delta>)
        {
            static_assert(
                requires(const value_type& value) {
                    {
                        value + component.delta_evaluate(solution, move)
                    } -> std::same_as<value_type>;
                },
                "co-located delta evaluation must satisfy Value + "
                "component.delta_evaluate(solution, move) -> Value");
            return component.delta_evaluate(solution, move);
        }
        else
        {
            static_assert(
                requires(const value_type& value) {
                    {
                        value + delta.delta_evaluate(solution, move)
                    } -> std::same_as<value_type>;
                },
                "delta evaluation must satisfy Value + "
                "delta.delta_evaluate(solution, move) -> Value");
            return delta.delta_evaluate(solution, move);
        }
    };

    // The delta law for one move of solution, valid or not.
    std::size_t checked = 0;
    const auto check_move =
        [&](const solution_type& solution,
            const value_type& current,
            const std::string& from,
            const std::size_t index,
            const move_type& move) {
            const auto label = [&] { return move_label(index, move) + " from " + from; };
            guarded(report, "delta law", label, [&] {
                if (!static_cast<bool>(neighborhood.is_valid(solution, move)))
                    return;
                ++checked;

                const value_type incremental = current + delta_of(solution, move);

                auto candidate = solution;
                neighborhood.make_move(candidate, move);
                const auto valid =
                    static_cast<bool>(solution_manager.is_valid(candidate));
                report.check(valid, "move application", [&] {
                    return label() + ": make_move produced an invalid Solution";
                });
                if (!valid)
                    return;

                const auto full = component.evaluate(candidate);
                report.check(fixture.equivalent(incremental, full), "delta law", [&] {
                    return label() + ": value + delta is " + value_text(incremental)
                        + ", the component after the move " + value_text(full)
                        + " (value before the move " + value_text(current) + ")";
                });
            });
        };

    std::mt19937_64 rng{fixture.options().seed};
    for_each_start(
        solution_manager,
        neighborhood,
        fixture.solution(),
        "the fixture Solution",
        fixture.options(),
        rng,
        [&](const solution_type& solution, const std::string& from) {
            std::optional<value_type> current;
            if (!guarded(report, "evaluation", "evaluating " + from, [&] {
                    current.emplace(component.evaluate(solution));
                }))
                return;

            if constexpr (deterministic_neighborhood_for<NHE, solution_type>)
            {
                std::vector<move_type> moves;
                if (!guarded(
                        report,
                        "move enumeration",
                        "enumerating the moves of " + from,
                        [&] {
                            moves = sample_moves(
                                neighborhood,
                                solution,
                                fixture.options().max_enumerated_moves,
                                rng);
                        }))
                    return;
                for (std::size_t index = 0; index < moves.size(); ++index)
                    check_move(solution, *current, from, index, moves[index]);
            }
            else
            {
                static_assert(
                    random_neighborhood_for<NHE, solution_type, std::mt19937_64>,
                    "the delta check needs moves: the NeighborhoodExplorer must "
                    "enumerate or sample them");
                for (std::size_t sample = 0; sample < fixture.options().random_samples;
                    ++sample)
                {
                    std::optional<move_type> move;
                    if (!guarded(
                            report,
                            "random proposal",
                            "drawing a move of " + from,
                            [&] {
                                move =
                                    easylocal::random_move(neighborhood, solution, rng);
                            }))
                        break;
                    if (move)
                        check_move(solution, *current, from, sample, *move);
                }
            }
        });

    report.check(
        checked != 0,
        "sample move",
        "the NeighborhoodExplorer produced no valid Move for the fixture Solution");

    return report;
}

} // namespace detail

/// Checks a separate delta cost component: for the valid moves of the fixture
/// Solution and of the random solutions of the options (enumerated, or sampled
/// when the neighborhood cannot enumerate), value + delta equals the
/// component's value after the move.
///
/// A failure names the move, the solution it starts from, value + delta and
/// the component's value; an exception of a hook is a failure too.
template<check_fixture Fixture, class NHE, class Component, class Delta>
[[nodiscard]] check_report check_delta_cost_component(
    const Fixture& fixture,
    const NHE& neighborhood,
    const Component& component,
    const Delta& delta)
{
    return detail::check_delta_law(fixture, neighborhood, component, delta);
}

/// Checks the component's own delta_evaluate (co-located): for the valid moves
/// of the fixture Solution and of the random solutions of the options, value +
/// delta equals the component's value after the move.
template<check_fixture Fixture, class NHE, class Component>
[[nodiscard]] check_report check_delta_cost_component(
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

/// The same, with every object built from the fixture; omit Delta for a
/// co-located delta.
template<class NHE, class Component, class Delta = void, check_fixture Fixture>
[[nodiscard]] check_report check_delta_cost_component(const Fixture& fixture)
{
    const auto neighborhood = detail::make_neighborhood<NHE>(fixture.solution_manager());
    const auto component = detail::make_from_input<Component>(fixture.input());
    if constexpr (std::is_void_v<Delta>)
    {
        return check_delta_cost_component(fixture, neighborhood, component);
    }
    else
    {
        return check_delta_cost_component(
            fixture,
            neighborhood,
            component,
            detail::make_from_input<Delta>(fixture.input()));
    }
}

} // namespace easylocal::testing
