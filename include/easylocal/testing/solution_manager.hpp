#pragma once

/// \file
/// check_solution_manager: the SolutionManager refers to the fixture Input,
/// and the fixture, initial and random solutions are valid.

#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/testing/check.hpp>
#include <easylocal/testing/fixture.hpp>

#include <cstddef>
#include <memory>
#include <random>

namespace easylocal::testing
{

/// The fixture's SolutionManager: bound to the fixture Input, and the fixture,
/// initial and random solutions are valid.
template<check_fixture Fixture>
[[nodiscard]] check_report check_solution_manager(const Fixture& fixture)
{
    using solution_manager_type = typename Fixture::solution_manager_type;
    const auto& solution_manager = fixture.solution_manager();

    check_report report{"SolutionManager"};

    report.check(
        std::addressof(solution_manager.input()) == std::addressof(fixture.input()),
        "input binding",
        "SolutionManager::input() must refer to the Input used to construct the manager");

    detail::check_fixture_solution(report, fixture);

    if constexpr (easylocal::has_initial_solution<solution_manager_type>)
    {
        const auto initial = solution_manager.initial_solution();
        report.check(
            static_cast<bool>(solution_manager.is_valid(initial)),
            "initial solution",
            "initial_solution() returned an invalid Solution");
    }

    if constexpr (easylocal::has_random_solution<solution_manager_type, std::mt19937_64>)
    {
        std::mt19937_64 rng{fixture.options().seed};
        for (std::size_t sample = 0; sample < fixture.options().random_samples; ++sample)
        {
            const auto random = solution_manager.random_solution(rng);
            report.check(
                static_cast<bool>(solution_manager.is_valid(random)),
                "random solution",
                "random_solution(rng) returned an invalid Solution");
        }
    }

    return report;
}

} // namespace easylocal::testing
