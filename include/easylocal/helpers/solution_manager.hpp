#pragma once

/// \file
/// SolutionManager: problem-side solution semantics (validity, evaluation,
/// optional construction capabilities).

#include <easylocal/utils/hash.hpp>

#include <concepts>
#include <cstdint>
#include <functional>

namespace easylocal
{

/// A SolutionManager: it declares `input_type` and `solution_type`, and has
/// `input()` and `is_valid(solution)`.
template<class SM>
concept base_solution_manager =
    requires(
        const SM& solution_manager,
        const typename SM::solution_type& solution)
    {
        typename SM::input_type;
        typename SM::solution_type;

        {
            solution_manager.input()
        } -> std::same_as<const typename SM::input_type&>;

        {
            solution_manager.is_valid(solution)
        } -> std::convertible_to<bool>;
    };

/// A SolutionManager with `initial_solution()`, which builds a solution
/// without randomness.
///
/// It is not part of the minimal contract: a runner starts from a given
/// solution, and the solvers and tools that build one require this or
/// has_random_solution, whose generator the caller owns, so that a seed
/// reproduces the solve.
template<class SM>
concept has_initial_solution =
    requires(const SM& solution_manager) {
        typename SM::solution_type;

        {
            solution_manager.initial_solution()
        } -> std::same_as<typename SM::solution_type>;
    };

/// A SolutionManager with `random_solution(rng)`, which draws a solution with
/// the generator `rng`.
template<class SM, class RNG>
concept has_random_solution =
    requires(const SM& solution_manager, RNG& rng) {
        typename SM::solution_type;

        {
            solution_manager.random_solution(rng)
        } -> std::same_as<typename SM::solution_type>;
    };

/// A SolutionManager with a `hash(solution)` member, the solution identity
/// that algorithms and tools use to recognize a solution met before (reactive
/// tabu search, search trajectories).
///
/// It takes precedence over the solution type's own std::hash, so that a
/// problem can leave out redundant data (caches, derived matrices) or identify
/// symmetric representations; an `equal(lhs, rhs)` member does the same for
/// operator==. Nothing in the framework requires them.
template<class SM>
concept has_solution_hash_member =
    requires(const SM& solution_manager, const typename SM::solution_type& solution) {
        { solution_manager.hash(solution) } -> std::convertible_to<std::uint64_t>;
    };

/// A SolutionManager whose solutions have a hash: its `hash(solution)` member,
/// or the solution type's `std::hash`.
template<class SM>
concept has_solution_hash =
    has_solution_hash_member<SM> || std_hashable<typename SM::solution_type>;

/// The hash of `solution`: the SolutionManager's `hash(solution)`, or else the
/// solution type's `std::hash`.
template<has_solution_hash SM>
[[nodiscard]]
constexpr std::uint64_t solution_hash(
    const SM& solution_manager,
    const typename SM::solution_type& solution)
{
    if constexpr (has_solution_hash_member<SM>)
    {
        return static_cast<std::uint64_t>(solution_manager.hash(solution));
    }
    else
    {
        return static_cast<std::uint64_t>(
            std::hash<typename SM::solution_type>{}(solution));
    }
}

/// A SolutionManager with an `equal(lhs, rhs)` member that compares solutions.
template<class SM>
concept has_solution_equality_member =
    requires(const SM& solution_manager, const typename SM::solution_type& solution) {
        { solution_manager.equal(solution, solution) } -> std::convertible_to<bool>;
    };

/// A SolutionManager whose solutions can be compared: its `equal(lhs, rhs)`
/// member, or the solution type's `==`.
template<class SM>
concept has_solution_equality = has_solution_equality_member<SM>
    || std::equality_comparable<typename SM::solution_type>;

/// Whether `lhs` and `rhs` are the same solution: the SolutionManager's
/// `equal(lhs, rhs)`, or else `lhs == rhs`.
template<has_solution_equality SM>
[[nodiscard]]
constexpr bool solutions_equal(
    const SM& solution_manager,
    const typename SM::solution_type& lhs,
    const typename SM::solution_type& rhs)
{
    if constexpr (has_solution_equality_member<SM>)
        return static_cast<bool>(solution_manager.equal(lhs, rhs));
    else
        return static_cast<bool>(lhs == rhs);
}

/// Optional non-virtual convenience base: associated types and the bound Input
/// reference.
///
/// Not required by the structural concepts above.
template<class Input, class Solution>
class solution_manager_base
{
public:
    /// The Input type.
    using input_type = Input;
    /// The Solution type.
    using solution_type = Solution;

    /// From the Input, which it keeps by reference.
    explicit solution_manager_base(const input_type& input) noexcept
        : input_{input}
    {
    }

    /// The Input.
    [[nodiscard]]
    const input_type& input() const noexcept
    {
        return input_;
    }

protected:
    /// The Input the SolutionManager was constructed from.
    const input_type& input_;
};

} // namespace easylocal
