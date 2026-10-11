#pragma once

/// \file
/// What an algorithm asks of the services it searches with, declared as tags.
///
/// A runner is registered on a neighborhood and a SolutionManager; when the
/// algorithm cannot search with them, the registration names the capability
/// that is missing instead of the concepts that failed. The tags only choose
/// the message: an algorithm that runs is never rejected because of them.

#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>

#include <concepts>
#include <type_traits>

namespace easylocal::runners
{

/// The capabilities an algorithm asks of the services it searches with.
namespace needs
{

/// The neighborhood explorer enumerates the moves of a solution.
struct enumerated_moves
{
};

/// The neighborhood explorer draws a random move of a solution.
struct random_moves
{
};

/// The SolutionManager builds a random solution.
struct random_solutions
{
};

/// The cost tells whether a cost is better than another.
struct strict_improvement
{
};

/// The cost tells whether a cost is better than or equivalent to another.
struct non_worsening
{
};

} // namespace needs

/// The capabilities an algorithm asks of the services it searches with.
///
/// An algorithm declares them as `search_requirements`, in the order they are
/// reported, the neighborhood before the cost:
/// \code
/// using search_requirements = easylocal::runners::requirements<
///     easylocal::runners::needs::random_moves,
///     easylocal::runners::needs::non_worsening>;
/// \endcode
/// A registration that cannot run then names the first capability of the list
/// the services do not provide. Declaring none, or none of the capabilities
/// that are missing, leaves the generic message.
template<class... Needs>
struct requirements
{
};

} // namespace easylocal::runners

namespace easylocal::runners::detail
{

// The requirements an algorithm declares, or none.
template<class Algorithm>
struct declared_requirements
{
    using type = requirements<>;
};

template<class Algorithm>
    requires requires { typename Algorithm::search_requirements; }
struct declared_requirements<Algorithm>
{
    using type = typename Algorithm::search_requirements;
};

template<class Algorithm>
using declared_requirements_t = typename declared_requirements<Algorithm>::type;

// Whether the services of Context provide the capability, with RNG where the
// capability draws at random; RNG is taken as it is passed, by value or by
// reference.
template<class Need, class Context, class RNG>
consteval bool provides()
{
    using generator_type = std::remove_cvref_t<RNG>;

    if constexpr (std::same_as<Need, needs::enumerated_moves>)
        return neighborhood_moves_context<Context>;
    else if constexpr (std::same_as<Need, needs::random_moves>)
        return random_move_context<Context, generator_type>;
    else if constexpr (std::same_as<Need, needs::random_solutions>)
        return easylocal::has_random_solution<
            typename Context::solution_manager_type,
            generator_type>;
    else if constexpr (std::same_as<Need, needs::strict_improvement>)
        return strict_improvement_context<Context>;
    else if constexpr (std::same_as<Need, needs::non_worsening>)
        return non_worsening_context<Context>;
    else
        return true;
}

// The message of the capability the services do not provide.
template<class Need, class Context, class RNG>
consteval void report_missing()
{
    if constexpr (std::same_as<Need, needs::enumerated_moves>)
    {
        static_assert(
            provides<Need, Context, RNG>(),
            "this algorithm scans the neighborhood of a solution: its explorer "
            "needs moves(), or first_move() and next_move()");
    }
    else if constexpr (std::same_as<Need, needs::random_moves>)
    {
        static_assert(
            provides<Need, Context, RNG>(),
            "this algorithm draws random moves: its explorer needs "
            "random_move(solution, rng)");
    }
    else if constexpr (std::same_as<Need, needs::random_solutions>)
    {
        static_assert(
            provides<Need, Context, RNG>(),
            "this algorithm starts from random solutions: its SolutionManager "
            "needs random_solution(rng)");
    }
    else if constexpr (std::same_as<Need, needs::strict_improvement>)
    {
        static_assert(
            provides<Need, Context, RNG>(),
            "this algorithm follows the cost down: its cost needs a compare, "
            "which gives better()");
    }
    else if constexpr (std::same_as<Need, needs::non_worsening>)
    {
        static_assert(
            provides<Need, Context, RNG>(),
            "this algorithm accepts a move that does not worsen the cost: its "
            "cost needs a compare with an equivalence, which gives "
            "better_or_equivalent()");
    }
}

// Whether the services of Context provide every capability the algorithm
// declares: a registration that provides them all, and still does not run,
// keeps the generic message.
template<class Context, class RNG, class... Needs>
consteval bool provides_all(requirements<Needs...>)
{
    return (provides<Needs, Context, RNG>() && ...);
}

template<class Algorithm, class Context, class RNG>
consteval bool provides_declared()
{
    return provides_all<Context, RNG>(declared_requirements_t<Algorithm>{});
}

// The message of the first capability of the list the services do not
// provide: one message, the neighborhood's before the cost's.
template<class Context, class RNG, class Need, class... Rest>
consteval void report_first_missing(requirements<Need, Rest...>)
{
    if constexpr (!provides<Need, Context, RNG>())
        report_missing<Need, Context, RNG>();
    else
        report_first_missing<Context, RNG>(requirements<Rest...>{});
}

template<class Context, class RNG>
consteval void report_first_missing(requirements<>)
{
}

// What the algorithm asks of the services of Context and they do not provide,
// called where the algorithm does not run on them.
template<class Algorithm, class Context, class RNG>
consteval void report_missing_requirements()
{
    report_first_missing<Context, RNG>(declared_requirements_t<Algorithm>{});
}

} // namespace easylocal::runners::detail
