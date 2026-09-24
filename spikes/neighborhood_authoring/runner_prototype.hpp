#pragma once

#include <concepts>
#include <cstddef>
#include <optional>
#include <utility>

namespace easylocal::spike::neighborhood_authoring
{

enum class prototype_termination
{
    local_optimum,
    evaluation_budget_exhausted,
};

template<class Solution, class Cost>
struct prototype_result
{
    Solution solution;
    Cost cost;
    std::size_t evaluations{};
    std::size_t traversals{};
    std::size_t full_scans{};
    std::size_t accepted_moves{};
    prototype_termination termination{};
};

struct raw_cursor_traversal
{
    template<class Neighborhood, class Solution, class Visitor>
    [[nodiscard]]
    static auto scan(
        const Neighborhood& neighborhood,
        const Solution& solution,
        Visitor&& visitor) -> bool
    {
        typename Neighborhood::move_type move{};

        if (!neighborhood.first_move(solution, move))
        {
            return true;
        }

        do
        {
            if (!visitor(move))
            {
                return false;
            }
        }
        while (neighborhood.next_move(solution, move));

        return true;
    }
};

struct range_traversal
{
    template<class Neighborhood, class Solution, class Visitor>
    [[nodiscard]]
    static auto scan(
        const Neighborhood& neighborhood,
        const Solution& solution,
        Visitor&& visitor) -> bool
    {
        for (const auto& move : neighborhood.moves(solution))
        {
            if (!visitor(move))
            {
                return false;
            }
        }

        return true;
    }
};

namespace detail
{

template<class Context>
concept prototype_improvement_context =
    requires(
        const Context& context,
        const typename Context::cost_type& candidate,
        const typename Context::cost_type& reference)
    {
        {
            context.better(candidate, reference)
        } -> std::convertible_to<bool>;
    };

} // namespace detail

template<class Traversal>
class prototype_first_improvement
{
public:
    explicit prototype_first_improvement(
        const std::size_t max_evaluations) noexcept
        : max_evaluations_{max_evaluations}
    {
    }

    template<class Context>
        requires detail::prototype_improvement_context<Context>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution) const
    {
        const auto& neighborhood = context.neighborhood_explorer();
        const auto evaluation = context.evaluation();

        using solution_type = typename Context::solution_type;
        using cost_type = typename Context::cost_type;
        using result_type = prototype_result<solution_type, cost_type>;

        auto current = evaluation.evaluate(solution);
        std::size_t evaluations = 1;
        std::size_t traversals = 0;
        std::size_t full_scans = 0;
        std::size_t accepted_moves = 0;

        while (true)
        {
            ++traversals;
            bool improved = false;
            bool budget_exhausted = false;

            const auto exhausted = Traversal::scan(
                neighborhood,
                solution,
                [&](const auto& move) {
                    if (evaluations == max_evaluations_)
                    {
                        budget_exhausted = true;
                        return false;
                    }

                    auto candidate =
                        evaluation.after_move(solution, current, move);
                    ++evaluations;

                    if (context.better(candidate.cost(), current.cost()))
                    {
                        evaluation.accept(
                            solution,
                            current,
                            std::move(candidate));
                        ++accepted_moves;
                        improved = true;
                        return false;
                    }

                    return true;
                });

            if (budget_exhausted)
            {
                return result_type{
                    .solution = std::move(solution),
                    .cost = current.cost(),
                    .evaluations = evaluations,
                    .traversals = traversals,
                    .full_scans = full_scans,
                    .accepted_moves = accepted_moves,
                    .termination = prototype_termination::
                        evaluation_budget_exhausted,
                };
            }

            if (exhausted)
            {
                ++full_scans;
            }

            if (!improved)
            {
                return result_type{
                    .solution = std::move(solution),
                    .cost = current.cost(),
                    .evaluations = evaluations,
                    .traversals = traversals,
                    .full_scans = full_scans,
                    .accepted_moves = accepted_moves,
                    .termination = prototype_termination::local_optimum,
                };
            }
        }
    }

private:
    std::size_t max_evaluations_;
};

template<class Traversal>
class prototype_best_improvement
{
public:
    explicit prototype_best_improvement(
        const std::size_t max_evaluations) noexcept
        : max_evaluations_{max_evaluations}
    {
    }

    template<class Context>
        requires detail::prototype_improvement_context<Context>
    [[nodiscard]]
    auto run(
        const Context& context,
        typename Context::solution_type solution) const
    {
        const auto& neighborhood = context.neighborhood_explorer();
        const auto evaluation = context.evaluation();

        using solution_type = typename Context::solution_type;
        using cost_type = typename Context::cost_type;
        using result_type = prototype_result<solution_type, cost_type>;
        using candidate_type = typename decltype(evaluation)::candidate_type;

        auto current = evaluation.evaluate(solution);
        std::size_t evaluations = 1;
        std::size_t traversals = 0;
        std::size_t full_scans = 0;
        std::size_t accepted_moves = 0;

        while (true)
        {
            ++traversals;
            std::optional<candidate_type> best_candidate;
            auto best_cost = current.cost();
            bool budget_exhausted = false;

            const auto exhausted = Traversal::scan(
                neighborhood,
                solution,
                [&](const auto& move) {
                    if (evaluations == max_evaluations_)
                    {
                        budget_exhausted = true;
                        return false;
                    }

                    auto candidate =
                        evaluation.after_move(solution, current, move);
                    ++evaluations;

                    if (context.better(candidate.cost(), best_cost))
                    {
                        best_cost = candidate.cost();
                        best_candidate = std::move(candidate);
                    }

                    return true;
                });

            if (budget_exhausted)
            {
                return result_type{
                    .solution = std::move(solution),
                    .cost = current.cost(),
                    .evaluations = evaluations,
                    .traversals = traversals,
                    .full_scans = full_scans,
                    .accepted_moves = accepted_moves,
                    .termination = prototype_termination::
                        evaluation_budget_exhausted,
                };
            }

            if (exhausted)
            {
                ++full_scans;
            }

            if (!best_candidate.has_value())
            {
                return result_type{
                    .solution = std::move(solution),
                    .cost = current.cost(),
                    .evaluations = evaluations,
                    .traversals = traversals,
                    .full_scans = full_scans,
                    .accepted_moves = accepted_moves,
                    .termination = prototype_termination::local_optimum,
                };
            }

            evaluation.accept(
                solution,
                current,
                std::move(*best_candidate));
            ++accepted_moves;
        }
    }

private:
    std::size_t max_evaluations_;
};

} // namespace easylocal::spike::neighborhood_authoring
