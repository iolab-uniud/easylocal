#pragma once

// The requirements runners place on a search context: a neighborhood, an
// evaluation and cost relations, plus the variants needed by each algorithm
// (enumerable moves, random moves, strict improvement).

#include <easylocal/cost/semantics.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>

#include <concepts>
#include <random>
#include <type_traits>
#include <utility>

namespace easylocal::runners::detail
{

template<class Context>
concept search_context_shape =
    requires(const Context& context)
    {
        typename Context::solution_type;
        typename Context::cost_type;
        typename Context::neighborhood_explorer_type;
        typename Context::neighborhood_explorer_type::move_type;

        {
            context.neighborhood_explorer()
        } -> std::same_as<const typename Context::neighborhood_explorer_type&>;

        context.evaluation();
    };

template<class Context>
using context_evaluation_type =
    std::remove_cvref_t<decltype(std::declval<const Context&>().evaluation())>;

// An evaluation facility of the solutions and moves of Context: evaluate,
// evaluate_move and commit, with evaluations and candidates that have the
// context's cost.
template<class Evaluation, class Context>
concept evaluation_facility_for = search_context_shape<Context>
    && requires(
        Evaluation& evaluation,
        const typename Context::solution_type& solution,
        typename Context::solution_type& mutable_solution,
        const typename Context::neighborhood_explorer_type::move_type& move,
        typename Evaluation::evaluation_type& current,
        typename Evaluation::candidate_type&& candidate) {
           {
               evaluation.evaluate(solution)
           } -> std::same_as<typename Evaluation::evaluation_type>;

           {
               evaluation.evaluate_move(solution, current, move)
           } -> std::same_as<typename Evaluation::candidate_type>;

           { current.cost() } -> std::same_as<const typename Context::cost_type&>;

           { candidate.cost() } -> std::same_as<const typename Context::cost_type&>;

           {
               evaluation.commit(mutable_solution, current, std::move(candidate))
           } -> std::same_as<void>;
       };

template<class Context>
concept search_context = search_context_shape<Context>
    && evaluation_facility_for<context_evaluation_type<Context>, Context>;

template<class Context>
concept neighborhood_moves_context =
    search_context<Context> &&
    requires(
        const Context& context,
        const typename Context::solution_type& solution)
    {
        {
            easylocal::moves(context.neighborhood_explorer(), solution)
        } -> easylocal::move_input_range_for<
            typename Context::neighborhood_explorer_type::move_type>;
    };

template<class Context>
concept strict_improvement_context =
    search_context<Context> &&
    requires(
        const Context& context,
        const typename Context::cost_type& candidate,
        const typename Context::cost_type& reference)
    {
        {
            context.better(candidate, reference)
        } -> std::convertible_to<bool>;
    };

template<class Context>
concept enumerating_strict_improvement_context =
    strict_improvement_context<Context> &&
    neighborhood_moves_context<Context>;

template<class Context>
concept non_worsening_context = strict_improvement_context<Context>
    && requires(
        const Context& context,
        const typename Context::cost_type& candidate,
        const typename Context::cost_type& reference) {
           {
               context.better_or_equivalent(candidate, reference)
           } -> std::convertible_to<bool>;
       };

// Whether the cost semantics of Context agree with the sign of cost::delta,
// as far as the compiler can tell: false only for a root compare that
// provably finds a larger arithmetic cost better.
template<class Context>
consteval bool delta_agrees_with_better()
{
    if constexpr (requires { typename Context::solution_manager_type; })
        return !easylocal::cost::detail::compare_contradicts_delta<
            typename Context::solution_manager_type>();
    else
        return true;
}

template<class Context, class RNG>
concept random_move_context =
    search_context<Context> && std::uniform_random_bit_generator<RNG>
    && easylocal::random_neighborhood_for<
        typename Context::neighborhood_explorer_type,
        typename Context::solution_type,
        RNG>;

} // namespace easylocal::runners::detail
