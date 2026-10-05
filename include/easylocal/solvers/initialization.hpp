#pragma once

/// \file How a solver builds its initial solutions: initialization::initial,
/// initialization::random or initialization::automatic, compile-time tags
/// checked against the SolutionManager, and detail::solver_start, the part of
/// the solvers that keeps the choice, the RNG and their builders.

#include <easylocal/solvers/solver.hpp>

#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace easylocal
{

namespace initialization
{

/// The tag of the initial solution, from `initial_solution()`.
struct Initial
{
};

/// The tag of a random initial solution, from `random_solution(rng)`.
struct Random
{
};

/// The tag of the default initialization: random when the SolutionManager
/// builds random solutions, else its initial solution.
struct Automatic
{
};

/// Starts from the SolutionManager's `initial_solution()`.
inline constexpr Initial initial{};
/// Starts from the SolutionManager's `random_solution(rng)`, with the solver's
/// RNG.
inline constexpr Random random{};
/// Starts from a random solution when the SolutionManager builds one, else
/// from its initial solution: the default of every solver.
inline constexpr Automatic automatic{};

} // namespace initialization

namespace detail
{

// The initialization chosen with a tag, kept by a solver or a stage.
enum class initialization_kind
{
    automatic,
    initial,
    random,
};

// An initialization tag a bound runner supports with RNG, checked at compile
// time: initial needs initial_solution(), random needs random_solution(rng),
// automatic needs either.
template<class Initialization, class BoundRunner, class RNG>
concept accepted_initialization =
    (std::same_as<std::remove_cvref_t<Initialization>, initialization::Initial>
        && bound_runner_with_initial_solution<BoundRunner>)
    || (std::same_as<std::remove_cvref_t<Initialization>, initialization::Random>
        && bound_runner_with_random_solution<BoundRunner, RNG>)
    || (std::same_as<std::remove_cvref_t<Initialization>, initialization::Automatic>
        && (bound_runner_with_initial_solution<BoundRunner>
            || bound_runner_with_random_solution<BoundRunner, RNG>));

[[nodiscard]]
constexpr initialization_kind initialization_kind_of(initialization::Initial) noexcept
{
    return initialization_kind::initial;
}

[[nodiscard]]
constexpr initialization_kind initialization_kind_of(initialization::Random) noexcept
{
    return initialization_kind::random;
}

[[nodiscard]]
constexpr initialization_kind initialization_kind_of(initialization::Automatic) noexcept
{
    return initialization_kind::automatic;
}

// A new solution of bound_runner, as kind says: automatic is random when the
// bound runner builds random solutions with RNG. The kind was checked against
// the bound runner when it was chosen.
template<class BoundRunner, class RNG>
[[nodiscard]]
typename BoundRunner::solution_type make_start_solution(
    const initialization_kind kind,
    const BoundRunner& bound_runner,
    RNG& rng)
{
    constexpr bool with_initial = bound_runner_with_initial_solution<BoundRunner>;
    constexpr bool with_random = bound_runner_with_random_solution<BoundRunner, RNG>;
    static_assert(
        with_initial || with_random,
        "a solver needs a SolutionManager with initial_solution() or "
        "random_solution(rng)");
    if constexpr (with_initial && with_random)
    {
        if (kind == initialization_kind::initial)
            return bound_runner.initial_solution();
        return bound_runner.random_solution(rng);
    }
    else if constexpr (with_random)
        return bound_runner.random_solution(rng);
    else
        return bound_runner.initial_solution();
}

// What a builder of a solver returns: a reference to the solver on an lvalue,
// the moved solver on a temporary.
template<class Self>
using builder_result_t = std::conditional_t<
    std::is_lvalue_reference_v<Self>,
    std::remove_reference_t<Self>&,
    std::remove_cvref_t<Self>>;

// The start of a solver whose runs start from solutions of BoundRunner: its
// RNG, its initialization, and their builders seed() and initialization().
template<class BoundRunner, class RNG>
class solver_start
{
public:
    static constexpr bool supports_initial =
        bound_runner_with_initial_solution<BoundRunner>;
    static constexpr bool supports_random =
        bound_runner_with_random_solution<BoundRunner, RNG>;

    template<class Self>
        requires std::constructible_from<RNG, std::uint64_t>
    builder_result_t<Self> seed(this Self&& self, const std::uint64_t seed)
    {
        solver_start& start = self;
        start.rng_ = RNG{seed};
        return std::forward<Self>(self);
    }

    template<class Self, class Initialization>
        requires accepted_initialization<Initialization, BoundRunner, RNG>
    builder_result_t<Self> initialization(
        this Self&& self,
        const Initialization initialization)
    {
        solver_start& start = self;
        start.kind_ = initialization_kind_of(initialization);
        return std::forward<Self>(self);
    }

    template<class Self>
    [[nodiscard]]
    auto& rng(this Self&& self) noexcept
    {
        return self.rng_;
    }

protected:
    explicit solver_start(RNG rng, const initialization_kind kind = {})
        : rng_{std::move(rng)}, kind_{kind}
    {
    }

    // A new initial solution, with the solver's RNG.
    [[nodiscard]]
    typename BoundRunner::solution_type make_initial_solution(
        const BoundRunner& bound_runner)
    {
        return make_start_solution(kind_, bound_runner, rng_);
    }

    [[nodiscard]]
    initialization_kind kind() const noexcept
    {
        return kind_;
    }

    RNG rng_;
    initialization_kind kind_;
};

} // namespace detail

} // namespace easylocal
