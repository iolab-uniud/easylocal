#pragma once

/// \file
/// How a solver builds the initial solution: initialization::initial or
/// initialization::random, as compile-time tags or as a runtime Mode, and
/// detail::InitializationSupport, the part of the solvers that keeps the choice.

#include <easylocal/solvers/solver.hpp>

#include <concepts>
#include <stdexcept>
#include <type_traits>

namespace easylocal
{

namespace initialization
{

/// Static tags are useful when initialization is fixed by the program: an
/// unsupported choice is then rejected at compile time.
struct Initial
{
};

struct Random
{
};

inline constexpr Initial initial{};
inline constexpr Random random{};

/// Mode is the runtime-facing counterpart, suitable for CLI/configuration.
/// Unsupported runtime selections are rejected explicitly; there is never an
/// implicit fallback from one initialization mode to another.
enum class Mode
{
    initial,
    random,
};

} // namespace initialization

namespace detail
{

// An initialization a solver over BoundRunner accepts: a tag the runner
// supports, checked at compile time, or a Mode, checked when it is given.
template<class Initialization, class BoundRunner, class RNG>
concept accepted_initialization =
    (std::same_as<std::remove_cvref_t<Initialization>, initialization::Initial>
        && bound_runner_with_initial_solution<BoundRunner>)
    || (std::same_as<std::remove_cvref_t<Initialization>, initialization::Random>
        && bound_runner_with_random_solution<BoundRunner, RNG>)
    || std::same_as<std::remove_cvref_t<Initialization>, initialization::Mode>;

// The initialization of a solver whose runs start from solutions of
// BoundRunner: which modes it supports, the selected one, and the initial
// solution it builds. The solver owns the RNG and passes it in.
template<class BoundRunner, class RNG>
class InitializationSupport
{
public:
    static constexpr bool supports_initial =
        bound_runner_with_initial_solution<BoundRunner>;
    static constexpr bool supports_random =
        bound_runner_with_random_solution<BoundRunner, RNG>;

    [[nodiscard]]
    static constexpr bool supports(const initialization::Mode mode) noexcept
    {
        switch (mode)
        {
        case initialization::Mode::initial:
            return supports_initial;
        case initialization::Mode::random:
            return supports_random;
        }
        return false;
    }

    [[nodiscard]]
    initialization::Mode initialization_mode() const noexcept
    {
        return mode_;
    }

    // Throws std::invalid_argument when the mode is not supported.
    void initialization_mode(const initialization::Mode mode)
    {
        validate(mode);
        mode_ = mode;
    }

protected:
    template<class Initialization>
        requires accepted_initialization<Initialization, BoundRunner, RNG>
    explicit InitializationSupport(const Initialization initialization)
        : mode_{to_mode(initialization)}
    {
        validate(mode_);
    }

    [[nodiscard]]
    typename BoundRunner::solution_type make_initial_solution(
        const BoundRunner& bound_runner,
        RNG& rng) const
    {
        switch (mode_)
        {
        case initialization::Mode::initial:
            if constexpr (supports_initial)
                return bound_runner.initial_solution();
            break;
        case initialization::Mode::random:
            if constexpr (supports_random)
                return bound_runner.random_solution(rng);
            break;
        }
        // The constructor and the setter validate the mode: a guard in case a
        // mode is added.
        throw std::logic_error{"unsupported Solver initialization mode"};
    }

private:
    static constexpr initialization::Mode to_mode(const initialization::Initial) noexcept
    {
        return initialization::Mode::initial;
    }

    static constexpr initialization::Mode to_mode(const initialization::Random) noexcept
    {
        return initialization::Mode::random;
    }

    static constexpr initialization::Mode to_mode(
        const initialization::Mode mode) noexcept
    {
        return mode;
    }

    static void validate(const initialization::Mode mode)
    {
        if (!supports(mode))
        {
            throw std::invalid_argument{
                mode == initialization::Mode::initial
                    ? "initial solution initialization is not supported by this Solver"
                    : "random solution initialization is not supported by this Solver"};
        }
    }

    initialization::Mode mode_;
};

} // namespace detail

} // namespace easylocal
