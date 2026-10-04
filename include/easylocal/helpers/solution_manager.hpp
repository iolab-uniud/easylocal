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

/// Optional SolutionManager construction capabilities.
///
/// They deliberately do not participate in the minimal base_solution_manager
/// contract: a Runner starts from an existing solution, while a Solver may
/// choose to require one of these capabilities when it owns solution
/// initialization.
///
/// Both operations are observed through a const SolutionManager. Randomness is
/// supplied explicitly by the caller so that the future Solver layer can own
/// seeding and RNG state without hidden per-service engines.
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

/// Optional solution identity, for algorithms and tools that recognize a
/// solution met before (reactive tabu search, search trajectories): a hash,
/// equal for equal solutions, and an equality.
///
/// A SolutionManager member takes precedence over the solution type's own
/// std::hash or operator==, so that a problem can leave out redundant data
/// (caches, derived matrices) or identify symmetric representations. Nothing in
/// the framework requires them.
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
