#pragma once

// check_cost_component: a cost component evaluates the fixture Solution, and
// the same value every time.

#include <easylocal/testing/check.hpp>
#include <easylocal/testing/fixture.hpp>

#include <concepts>

namespace easylocal::testing
{

// A cost component evaluates the fixture Solution, and evaluating it twice
// gives equivalent values.
template<check_fixture Fixture, class Component>
[[nodiscard]] check_report check_cost_component(
    const Fixture& fixture,
    const Component& component)
{
    using solution_type = typename Fixture::solution_type;

    static_assert(
        requires(const Component& c, const solution_type& solution) {
            c.evaluate(solution);
        },
        "the cost component must provide evaluate(const Solution&) const");

    check_report report{"cost component"};

    if (!detail::check_fixture_solution(report, fixture))
    {
        return report;
    }

    const auto first = component.evaluate(fixture.solution());
    report.check(
        true,
        "evaluation",
        "evaluate(const Solution&) completed for the fixture Solution");

    if constexpr (requires { fixture.equivalent(first, first); })
    {
        const auto second = component.evaluate(fixture.solution());
        report.check(
            fixture.equivalent(first, second),
            "repeat evaluation",
            "evaluating the same Solution twice produced non-equivalent component values");
    }

    return report;
}

// The same, with the component built from the fixture Input.
template<class Component, check_fixture Fixture>
[[nodiscard]] check_report check_cost_component(const Fixture& fixture)
{
    return check_cost_component(
        fixture,
        detail::make_from_input<Component>(fixture.input()));
}

} // namespace easylocal::testing
