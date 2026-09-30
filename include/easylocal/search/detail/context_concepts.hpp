#pragma once

#include <easylocal/cursor_moves.hpp>

#include <easylocal/neighborhood_concepts.hpp>

#include <concepts>
#include <type_traits>
#include <utility>

namespace easylocal::search::detail
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

template<class Context>
concept search_context =
    search_context_shape<Context> &&
    requires(
        const Context& context,
        const typename Context::solution_type& solution,
        typename Context::solution_type& mutable_solution,
        const typename Context::neighborhood_explorer_type::move_type& move,
        context_evaluation_type<Context> evaluation,
        typename context_evaluation_type<Context>::evaluation_type& current,
        typename context_evaluation_type<Context>::candidate_type&& candidate)
    {
        typename context_evaluation_type<Context>::evaluation_type;
        typename context_evaluation_type<Context>::candidate_type;

        {
            evaluation.evaluate(solution)
        } -> std::same_as<
            typename context_evaluation_type<Context>::evaluation_type>;

        {
            evaluation.evaluate_move(solution, current, move)
        } -> std::same_as<
            typename context_evaluation_type<Context>::candidate_type>;

        {
            current.cost()
        } -> std::same_as<const typename Context::cost_type&>;

        {
            candidate.cost()
        } -> std::same_as<const typename Context::cost_type&>;

        {
            evaluation.commit(
                mutable_solution,
                current,
                std::move(candidate))
        } -> std::same_as<void>;
    };

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

} // namespace easylocal::search::detail
