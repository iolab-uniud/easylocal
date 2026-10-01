#pragma once

#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/testing/check.hpp>

#include <cstddef>
#include <memory>
#include <type_traits>

namespace easylocal::testing
{

template<class Test>
[[nodiscard]] auto check_solution_manager() -> check_report
{
    using solution_manager_type = typename Test::solution_manager;
    using input_type = typename solution_manager_type::input_type;
    using solution_type = typename solution_manager_type::solution_type;

    static_assert(
        easylocal::base_solution_manager<solution_manager_type>,
        "Test::solution_manager does not satisfy the EasyLocal SolutionManager core contract");

    auto instance = Test::instance();
    static_assert(std::same_as<
        std::remove_cvref_t<decltype(instance)>,
        input_type>);

    auto solution_manager = detail::make_solution_manager<
        Test, input_type, solution_manager_type>(instance);

    auto solution = Test::solution(instance);
    static_assert(std::same_as<
        std::remove_cvref_t<decltype(solution)>,
        solution_type>);

    check_report report{"SolutionManager"};

    report.check(
        std::addressof(solution_manager.input()) == std::addressof(instance),
        "instance binding",
        "SolutionManager::instance() must refer to the Instance used to construct the manager");

    report.check(
        static_cast<bool>(solution_manager.is_valid(solution)),
        "fixture solution",
        "Test::solution(instance) must return a valid Solution");

    if constexpr (easylocal::has_initial_solution<solution_manager_type>)
    {
        const auto initial = solution_manager.initial_solution();
        report.check(
            static_cast<bool>(solution_manager.is_valid(initial)),
            "initial solution",
            "initial_solution() returned an invalid Solution");
    }

    if constexpr (easylocal::has_random_solution<
                      solution_manager_type,
                      deterministic_rng>)
    {
        deterministic_rng rng;
        for (std::size_t sample = 0;
             sample < detail::random_samples_v<Test>;
             ++sample)
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
