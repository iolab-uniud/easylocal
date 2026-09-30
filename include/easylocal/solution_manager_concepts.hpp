#pragma once

#include <concepts>

namespace easylocal
{


template<class SM>
concept base_solution_manager =
    requires(
        const SM& solution_manager,
        const typename SM::solution_type& solution)
    {
        typename SM::instance_type;
        typename SM::solution_type;

        {
            solution_manager.instance()
        } -> std::same_as<const typename SM::instance_type&>;

        {
            solution_manager.is_valid(solution)
        } -> std::convertible_to<bool>;
    };

template<class SM>
concept evaluable_solution_manager =
    base_solution_manager<SM> &&
    requires(
        const SM& solution_manager,
        const typename SM::solution_type& solution)
    {
        typename SM::cost_type;

        {
            solution_manager.evaluate(solution)
        } -> std::same_as<typename SM::cost_type>;
    };

// Optional SolutionManager construction capabilities. They deliberately do not
// participate in the minimal base_solution_manager contract: a Runner starts
// from an existing solution, while a Solver may choose to require one of these
// capabilities when it owns solution initialization.
//
// Both operations are observed through a const SolutionManager. Randomness is
// supplied explicitly by the caller so that the future Solver layer can own
// seeding and RNG state without hidden per-service engines.
template<class SM>
concept has_initial_solution =
    requires(const SM& solution_manager) {
        typename SM::solution_type;

        {
            solution_manager.initial_solution()
        } -> std::same_as<typename SM::solution_type>;
    };

template<class SM, class RNG>
concept has_random_solution =
    requires(const SM& solution_manager, RNG& rng) {
        typename SM::solution_type;

        {
            solution_manager.random_solution(rng)
        } -> std::same_as<typename SM::solution_type>;
    };

} // namespace easylocal
