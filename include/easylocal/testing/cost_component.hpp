#pragma once

#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/testing/check.hpp>

#include <concepts>
#include <type_traits>

namespace easylocal::testing
{

template<class Test>
[[nodiscard]] auto check_cost_component() -> check_report
{
    using solution_manager_type = detail::test_solution_manager_t<Test>;
    using component_type = typename Test::component;
    using input_type = typename solution_manager_type::input_type;
    using solution_type = typename solution_manager_type::solution_type;

    static_assert(
        easylocal::base_solution_manager<solution_manager_type>,
        "the check fixture does not name a valid SolutionManager");

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
    auto component = detail::make_component<
        Test, input_type, component_type>(instance);
    auto solution = Test::solution(instance);
    static_assert(std::same_as<
        std::remove_cvref_t<decltype(solution)>,
        solution_type>);

    check_report report{"cost component"};

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

    const auto first = component.evaluate(solution);
    report.check(
        true,
        "evaluation",
        "evaluate(const Solution&) completed for the fixture Solution");

    if constexpr (
        requires { Test::equivalent(first, first); } ||
        requires { { first == first } -> std::convertible_to<bool>; })
    {
        const auto second = component.evaluate(solution);
        report.check(
            detail::equivalent<Test>(first, second),
            "repeat evaluation",
            "evaluating the same Solution twice produced non-equivalent component values");
    }

    return report;
}

} // namespace easylocal::testing
