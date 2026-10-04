#pragma once

/// \file
/// ParetoLateAcceptanceHillClimbing: late acceptance for multi-objective
/// cost::pareto costs, over a history of solutions compared by dominance; the
/// result is the Pareto front of the run.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/utils/limit.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <memory>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::runners
{

/// The parameters of ParetoLateAcceptanceHillClimbing.
struct ParetoLateAcceptanceHillClimbingParameters
{
    /// Number of solutions in the history: the first is the initial solution,
    /// the others random ones.
    std::size_t history_length{20};
    /// Iterations after which the search stops as soon as the idle iterations
    /// exceed idle_ratio of all the iterations.
    std::size_t max_iterations{100000};
    /// Share of the iterations that may be idle once max_iterations is past.
    double idle_ratio{0.02};
    /// A candidate that does not dominate the current solution may still
    /// replace the next solution of the history if it dominates it.
    bool second_chance{true};
    /// Evaluation budget, including the initial evaluations; unlimited by
    /// default.
    limit max_evaluations{unlimited};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "history_length",
                &ParetoLateAcceptanceHillClimbingParameters::history_length>(
                "Number of solutions in the history",
                config::range(1, easylocal::unlimited)),
            config::field<
                "max_iterations",
                &ParetoLateAcceptanceHillClimbingParameters::max_iterations>(
                "Iterations after which the search may stop when mostly idle",
                config::range(0, easylocal::unlimited)),
            config::field<
                "idle_ratio",
                &ParetoLateAcceptanceHillClimbingParameters::idle_ratio>(
                "Share of idle iterations that ends the search after max_iterations",
                config::range(0.0, 1.0)),
            config::field<
                "second_chance",
                &ParetoLateAcceptanceHillClimbingParameters::second_chance>(
                "Let a candidate replace the next solution of the history it dominates"),
            config::field<
                "max_evaluations",
                &ParetoLateAcceptanceHillClimbingParameters::max_evaluations>(
                "Maximum number of solution evaluations, or unlimited",
                config::range(0, easylocal::unlimited)));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

/// Pareto Late Acceptance Hill Climbing (Da Ros and Di Gaspero), for a
/// cost::pareto cost: a history of solutions, the initial one and
/// history_length - 1 random ones, is visited in a circle.
///
/// At each iteration a random move of the current solution is evaluated; if the
/// candidate dominates the current solution it replaces it in the history, and
/// the search goes on from the next solution of the history. Otherwise, with
/// second_chance, a candidate that dominates the next solution replaces it, and
/// the search goes on from the solution it replaced, two positions on, as in
/// Da Ros's implementation; else it goes on from the next solution. Past
/// max_iterations it stops as soon as more than idle_ratio of the iterations
/// are idle (no replacement since the last one). The result is the front of the
/// run (search_run's archive of the non-dominated solutions reached), with its
/// first solution by objectives.
/// Requires a neighborhood explorer with random_move(), a SolutionManager with
/// random_solution() (has_random_solution) to fill the history, and a
/// cost::pareto cost.
class ParetoLateAcceptanceHillClimbing
{
public:
    /// The parameter block of the algorithm.
    using parameters_type = ParetoLateAcceptanceHillClimbingParameters;

    explicit ParetoLateAcceptanceHillClimbing(
        const ParetoLateAcceptanceHillClimbingParameters parameters) noexcept
        : parameters_{parameters}
    {
        assert(parameters_.validate());
    }

    /// Runs the search from solution, drawing random moves with rng.
    ///
    /// The bound runner calls it, with the run of its context (neighborhood,
    /// evaluation, cost relations). The run keeps the front of the
    /// non-dominated solutions (Run::archives_front).
    template<class Run, std::uniform_random_bit_generator RNG>
        requires Run::archives_front
        && detail::random_move_context<typename Run::context_type, RNG>
        && detail::search_context<typename Run::context_type>
        && has_random_solution<
            std::remove_cvref_t<decltype(std::declval<const Run&>().solution_manager())>,
            RNG>
    [[nodiscard]]
    auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
    {
        run.limit_evaluations(parameters_.max_evaluations);

        // Entries are immutable and shared: moving along the history copies
        // no solution.
        struct entry
        {
            typename Run::solution_type solution;
            typename Run::evaluation_type evaluation;
        };
        using entry_pointer = std::shared_ptr<const entry>;

        std::vector<entry_pointer> history;
        history.reserve(parameters_.history_length);
        auto initial = run.start(solution);
        history.push_back(
            std::make_shared<const entry>(std::move(solution), std::move(initial)));
        while (history.size() < parameters_.history_length && !run.should_stop())
        {
            auto random = run.solution_manager().random_solution(rng);
            auto evaluation = run.evaluate_solution(random);
            history.push_back(
                std::make_shared<const entry>(std::move(random), std::move(evaluation)));
        }

        // The current solution with the move applied, as a new entry.
        const auto applied =
            [&run](const entry& current, auto&& candidate, const auto& move) {
                auto next = current;
                run.commit(next.solution, next.evaluation, std::move(candidate), move);
                return std::make_shared<const entry>(std::move(next));
            };

        const auto size = history.size();
        std::size_t index = 0;
        std::size_t idle_iterations = 0;
        auto current = history[0];
        while (!run.should_stop())
        {
            if (run.iterations() >= parameters_.max_iterations
                && static_cast<double>(idle_iterations)
                    > parameters_.idle_ratio * static_cast<double>(run.iterations()))
            {
                return finish(run, termination_reason::idle_limit_reached);
            }

            auto move = run.random_move(current->solution, rng);
            if (!move.has_value())
                return finish(run, termination_reason::local_optimum);

            run.next_iteration();
            auto candidate =
                run.evaluate_move(current->solution, current->evaluation, *move);
            const auto next = (index + 1) % size;
            if (run.better(candidate.cost(), current->evaluation.cost()))
            {
                history[index] = applied(*current, std::move(candidate), *move);
                current = history[next];
                index = next;
                idle_iterations = 0;
            }
            else if (parameters_.second_chance
                && run.better(candidate.cost(), history[next]->evaluation.cost()))
            {
                // On from the solution replaced, not from the history: as in
                // the original algorithm.
                auto replacement = applied(*current, std::move(candidate), *move);
                current = std::exchange(history[next], std::move(replacement));
                index = (index + 2) % size;
                idle_iterations = 0;
            }
            else
            {
                current = history[next];
                index = next;
                ++idle_iterations;
            }
        }
        return finish(run);
    }

private:
    template<class Run, class... Reason>
    [[nodiscard]]
    static auto finish(Run& run, Reason... reason)
    {
        auto front = run.front().sorted();
        auto& first = front.front();
        return run.finish(std::move(first.solution), std::move(first.cost), reason...);
    }

    ParetoLateAcceptanceHillClimbingParameters parameters_;
};

} // namespace easylocal::runners
