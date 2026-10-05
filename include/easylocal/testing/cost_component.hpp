#pragma once

/// \file
/// check_cost_component: a cost component evaluates the fixture Solution, and
/// the same value every time.

#include <easylocal/testing/check.hpp>
#include <easylocal/testing/detail/support.hpp>
#include <easylocal/testing/fixture.hpp>

#include <concepts>
#include <type_traits>

namespace easylocal::testing
{

/// A cost component evaluates the fixture Solution, and evaluating it twice
/// gives equivalent values; an exception of evaluate() is a failure.
///
/// Values the fixture's Equivalent cannot compare are a compile error.
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

    using value_type =
        std::remove_cvref_t<decltype(component.evaluate(fixture.solution()))>;
    static_assert(
        requires(const value_type& value) { fixture.equivalent(value, value); },
        "the fixture's Equivalent cannot compare the values of the component: give "
        "the value operator==, or the fixture another Equivalent");

    detail::guarded(report, "repeat evaluation", "evaluating the fixture Solution", [&] {
        const auto first = component.evaluate(fixture.solution());
        const auto second = component.evaluate(fixture.solution());
        report.check(
            fixture.equivalent(first, second),
            "repeat evaluation",
            "evaluating the same Solution twice gave " + detail::value_text(first)
                + " and then " + detail::value_text(second));
    });

    return report;
}

/// The same, with the component built from the fixture Input.
template<class Component, check_fixture Fixture>
[[nodiscard]] check_report check_cost_component(const Fixture& fixture)
{
    return check_cost_component(
        fixture,
        detail::make_from_input<Component>(fixture.input()));
}

} // namespace easylocal::testing
