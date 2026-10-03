#pragma once

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/utils/detail/attributes.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <optional>
#include <random>
#include <utility>
#include <vector>

// Tabu Search: at each iteration the best admissible move of the neighborhood
// is applied, even when it worsens the cost. A tabu list forbids the moves that
// would undo recent ones (by the neighborhood's inverse), unless an aspiration
// criterion lifts the prohibition. The tabu lists live in runners::tabu, the
// aspiration criteria in runners::aspiration.
namespace easylocal::runners
{

// What a tabu list learns after each iteration: the move applied, the solution
// and its cost after it, the iteration (counted from 1) and whether the move
// improved the best cost.
template<class Solution, class Move, class Cost>
struct tabu_step
{
    const Move& move;
    const Solution& solution;
    const Cost& cost;
    std::size_t iteration;
    bool improved_best;
};

// A tabu list policy: a value holding its parameters, from which each run makes
// the list's state for its move type. The state answers tabu_tenure(is_inverse)
// for a candidate move, where is_inverse(tabu_move) tells whether the move is
// forbidden by a move the list holds: the iterations left before the move is no
// longer tabu, or nothing when it is admissible. update(step, rng) records an
// applied move.
template<class List, class Solution, class Move, class Cost, class RNG>
concept tabu_list_for =
    requires(const List& list) {
        typename List::parameters_type;
        { list.template make_state<Move>() };
    }
    && requires(
        decltype(std::declval<const List&>().template make_state<Move>())& state,
        const decltype(std::declval<const List&>().template make_state<Move>())&
            const_state,
        bool (*is_inverse)(const Move&),
        const tabu_step<Solution, Move, Cost>& step,
        RNG& rng) {
           {
               const_state.tabu_tenure(is_inverse)
           } -> std::same_as<std::optional<std::size_t>>;
           { state.update(step, rng) } -> std::same_as<void>;
       };

namespace tabu
{

struct FixedLengthParameters
{
    // Iterations a move stays in the list.
    std::size_t tenure{10};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"tenure", &FixedLengthParameters::tenure>(
                "Number of iterations a move stays tabu"));
    }

    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        if (tenure == 0)
            return config::validation_result::failure("tenure must be positive");
        return config::validation_result::success();
    }
};

// A list of the last tenure moves, overwritten in a circle: each move stays
// tabu for tenure iterations.
class FixedLength
{
public:
    using parameters_type = FixedLengthParameters;

    explicit FixedLength(const FixedLengthParameters& parameters) noexcept
        : parameters_{parameters}
    {
        assert(parameters_.validate());
    }

    template<class Move>
    class state
    {
    public:
        explicit state(const std::size_t tenure) : tenure_{tenure}
        {
            moves_.reserve(tenure);
        }

        // A move forbidden by several entries is tabu until the youngest
        // expires.
        template<class IsInverse>
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(IsInverse&& is_inverse) const
        {
            std::optional<std::size_t> tenure;
            for (std::size_t age = 0; age < moves_.size(); ++age)
            {
                // The entry inserted age iterations before the newest.
                const auto index = (newest_ + moves_.size() - age) % moves_.size();
                if (is_inverse(moves_[index]))
                {
                    tenure = tenure_ - age;
                    break;
                }
            }
            return tenure;
        }

        template<class Step, class RNG>
        void update(const Step& step, RNG&)
        {
            if (moves_.size() < tenure_)
            {
                moves_.push_back(step.move);
                newest_ = moves_.size() - 1;
            }
            else
            {
                newest_ = (newest_ + 1) % tenure_;
                moves_[newest_] = step.move;
            }
        }

    private:
        std::vector<Move> moves_;
        std::size_t newest_{};
        std::size_t tenure_;
    };

    template<class Move>
    [[nodiscard]]
    state<Move> make_state() const
    {
        return state<Move>{parameters_.tenure};
    }

private:
    FixedLengthParameters parameters_;
};

} // namespace tabu

namespace aspiration
{

// A tabu move is admitted when it would improve the best cost found.
struct ByObjective
{
    static constexpr bool needs_cost = true;

    template<class Run, class Cost>
    [[nodiscard]]
    bool overrides(const Run& run, const Cost& candidate, const Cost& best) const
    {
        return run.better(candidate, best);
    }
};

// Tabu moves are never admitted, and need not be evaluated.
struct None
{
    static constexpr bool needs_cost = false;

    template<class Run, class Cost>
    [[nodiscard]]
    bool overrides(const Run&, const Cost&, const Cost&) const
    {
        return false;
    }
};

} // namespace aspiration

template<class ListParameters>
struct TabuSearchParameters
{
    // Iterations without improving the best cost after which the search stops.
    std::size_t max_idle_iterations{1000};
    // 0: no limit.
    std::size_t max_iterations{0};
    // Evaluation budget, including the initial evaluation; 0 means no budget.
    std::size_t max_evaluations{0};
    ListParameters tabu_list{};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "max_idle_iterations",
                &TabuSearchParameters::max_idle_iterations>(
                "Maximum number of iterations without improving the best cost"),
            config::field<"max_iterations", &TabuSearchParameters::max_iterations>(
                "Maximum number of iterations (0: no limit)"),
            config::field<"max_evaluations", &TabuSearchParameters::max_evaluations>(
                "Maximum number of solution evaluations (0: no budget)"),
            config::group<"tabu_list", &TabuSearchParameters::tabu_list>(
                "The tabu list"));
    }

    // The tabu list is validated as a group.
    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        if (max_idle_iterations == 0)
        {
            return config::validation_result::failure(
                "max_idle_iterations must be positive");
        }
        return config::validation_result::success();
    }
};

template<class ListParameters>
struct FirstImprovementTabuSearchParameters
{
    std::size_t max_idle_iterations{1000};
    std::size_t max_iterations{0};
    std::size_t max_evaluations{0};
    // The scan stops at the first admissible move that improves the best cost
    // rather than the current one.
    bool improve_on_best{false};
    ListParameters tabu_list{};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "max_idle_iterations",
                &FirstImprovementTabuSearchParameters::max_idle_iterations>(
                "Maximum number of iterations without improving the best cost"),
            config::field<
                "max_iterations",
                &FirstImprovementTabuSearchParameters::max_iterations>(
                "Maximum number of iterations (0: no limit)"),
            config::field<
                "max_evaluations",
                &FirstImprovementTabuSearchParameters::max_evaluations>(
                "Maximum number of solution evaluations (0: no budget)"),
            config::field<
                "improve_on_best",
                &FirstImprovementTabuSearchParameters::improve_on_best>(
                "Stop the scan at a move improving the best cost, not the current one"),
            config::group<"tabu_list", &FirstImprovementTabuSearchParameters::tabu_list>(
                "The tabu list"));
    }

    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        if (max_idle_iterations == 0)
        {
            return config::validation_result::failure(
                "max_idle_iterations must be positive");
        }
        return config::validation_result::success();
    }
};

namespace detail
{

template<class Context>
concept tabu_search_context = enumerating_strict_improvement_context<Context>
    && inverse_neighborhood_for<
        typename Context::neighborhood_explorer_type,
        typename Context::solution_type>;

// The machinery common to the tabu searches; Select decides, for each
// admissible candidate, whether the scan stops at it.
template<class TabuList, class Aspiration>
class tabu_search_engine
{
public:
    template<class Parameters>
    tabu_search_engine(const Parameters& parameters, Aspiration aspiration)
        : max_idle_iterations_{parameters.max_idle_iterations},
          max_iterations_{parameters.max_iterations},
          max_evaluations_{parameters.max_evaluations},
          tabu_list_{parameters.tabu_list},
          aspiration_{std::move(aspiration)}
    {
    }

    template<class Run, class RNG, class StopAt>
    [[nodiscard]]
    auto search(Run& run, typename Run::solution_type solution, RNG& rng, StopAt stop_at)
        const
    {
        using move_type = typename Run::move_type;
        using cost_type = typename Run::cost_type;

        if (max_evaluations_ != 0)
            run.limit_evaluations(max_evaluations_);
        auto current = run.start(solution);
        auto best_solution = solution;
        auto best_cost = current.cost();
        auto list = tabu_list_.template make_state<move_type>();
        std::size_t idle_iterations = 0;

        while (!run.should_stop())
        {
            if (idle_iterations >= max_idle_iterations_)
            {
                return run.finish(
                    std::move(best_solution),
                    std::move(best_cost),
                    termination_reason::idle_limit_reached);
            }
            if (max_iterations_ != 0 && run.iterations() >= max_iterations_)
                return run.finish(std::move(best_solution), std::move(best_cost));

            // The chosen admissible candidate; ties share the choice
            // uniformly (each of k equivalent candidates replaces the choice
            // with probability 1/k).
            std::optional<typename Run::candidate_type> chosen;
            std::optional<move_type> chosen_move;
            std::size_t ties = 0;
            // The least tabu move, applied when no move is admissible.
            std::optional<move_type> least_tabu;
            std::size_t least_tenure = 0;
            std::size_t least_ties = 0;
            bool neighborhood_empty = true;

            for (const auto& move : run.moves(solution))
            {
                if (run.should_stop())
                    return run.finish(std::move(best_solution), std::move(best_cost));
                neighborhood_empty = false;

                const auto tenure = list.tabu_tenure([&](const move_type& tabu_move) {
                    return easylocal::inverse(
                        run.neighborhood_explorer(),
                        solution,
                        move,
                        tabu_move);
                });
                if (tenure.has_value())
                {
                    if (!least_tabu.has_value() || *tenure < least_tenure)
                    {
                        least_tabu = move;
                        least_tenure = *tenure;
                        least_ties = 1;
                    }
                    else if (*tenure == least_tenure && draw(rng, ++least_ties) == 0)
                    {
                        least_tabu = move;
                    }
                    if constexpr (!Aspiration::needs_cost)
                        continue;
                }

                auto candidate = run.evaluate_move(solution, current, move);
                if (tenure.has_value()
                    && !aspiration_.overrides(run, candidate.cost(), best_cost))
                {
                    continue;
                }

                const auto stop =
                    stop_at(run, candidate.cost(), current.cost(), best_cost);
                if (!chosen.has_value() || run.better(candidate.cost(), chosen->cost()))
                {
                    chosen = std::move(candidate);
                    chosen_move = move;
                    ties = 1;
                }
                else if (!run.better(chosen->cost(), candidate.cost())
                    && draw(rng, ++ties) == 0)
                {
                    chosen = std::move(candidate);
                    chosen_move = move;
                }
                if (stop)
                    break;
            }

            if (neighborhood_empty)
            {
                return run.finish(
                    std::move(best_solution),
                    std::move(best_cost),
                    termination_reason::local_optimum);
            }
            if (!chosen.has_value())
            {
                // Every move is tabu: the least tabu one is applied.
                assert(least_tabu.has_value());
                if (run.should_stop())
                    break;
                chosen = run.evaluate_move(solution, current, *least_tabu);
                chosen_move = std::move(least_tabu);
            }

            run.next_iteration();
            run.commit(solution, current, std::move(*chosen), *chosen_move);
            const bool improved = run.better(current.cost(), best_cost);
            if (improved)
            {
                const cost_type previous_best = best_cost;
                best_solution = solution;
                best_cost = current.cost();
                run.incumbent_updated(previous_best, best_cost);
                idle_iterations = 0;
            }
            else
            {
                ++idle_iterations;
            }
            list.update(
                tabu_step<typename Run::solution_type, move_type, cost_type>{
                    .move = *chosen_move,
                    .solution = solution,
                    .cost = current.cost(),
                    .iteration = run.iterations(),
                    .improved_best = improved},
                rng);
        }

        return run.finish(std::move(best_solution), std::move(best_cost));
    }

private:
    // Uniform in [0, count).
    template<class RNG>
    [[nodiscard]]
    static std::size_t draw(RNG& rng, const std::size_t count)
    {
        return std::uniform_int_distribution<std::size_t>{0, count - 1}(rng);
    }

    std::size_t max_idle_iterations_;
    std::size_t max_iterations_;
    std::size_t max_evaluations_;
    TabuList tabu_list_;
    EASYLOCAL_NO_UNIQUE_ADDRESS Aspiration aspiration_;
};

} // namespace detail

// Tabu Search exploring the whole neighborhood: the best admissible move is
// applied, ties broken uniformly at random. When every move is tabu, the least
// tabu one is applied. It stops after max_idle_iterations iterations without
// improving the best cost, and returns the best solution found.
template<class TabuList = tabu::FixedLength, class Aspiration = aspiration::ByObjective>
class TabuSearch
{
public:
    using parameters_type = TabuSearchParameters<typename TabuList::parameters_type>;

    explicit TabuSearch(const parameters_type& parameters, Aspiration aspiration = {})
        : engine_{parameters, std::move(aspiration)}
    {
        assert(parameters.validate());
    }

    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::tabu_search_context<typename Run::context_type>
        && tabu_list_for<
            TabuList,
            typename Run::solution_type,
            typename Run::move_type,
            typename Run::cost_type,
            RNG>
    [[nodiscard]]
    auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
    {
        return engine_.search(run, std::move(solution), rng, [](const auto&...) {
            return false;
        });
    }

private:
    detail::tabu_search_engine<TabuList, Aspiration> engine_;
};

// Tabu Search stopping the scan at the first admissible move that improves the
// current cost or, with improve_on_best, the best cost; without one, the best
// admissible move of the whole neighborhood is applied, as in TabuSearch.
template<class TabuList = tabu::FixedLength, class Aspiration = aspiration::ByObjective>
class FirstImprovementTabuSearch
{
public:
    using parameters_type =
        FirstImprovementTabuSearchParameters<typename TabuList::parameters_type>;

    explicit FirstImprovementTabuSearch(
        const parameters_type& parameters,
        Aspiration aspiration = {})
        : engine_{parameters, std::move(aspiration)},
          improve_on_best_{parameters.improve_on_best}
    {
        assert(parameters.validate());
    }

    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::tabu_search_context<typename Run::context_type>
        && tabu_list_for<
            TabuList,
            typename Run::solution_type,
            typename Run::move_type,
            typename Run::cost_type,
            RNG>
    [[nodiscard]]
    auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
    {
        return engine_.search(
            run,
            std::move(solution),
            rng,
            [improve_on_best = improve_on_best_](
                const Run& search,
                const auto& candidate,
                const auto& current,
                const auto& best) {
                return search.better(candidate, improve_on_best ? best : current);
            });
    }

private:
    detail::tabu_search_engine<TabuList, Aspiration> engine_;
    bool improve_on_best_;
};

} // namespace easylocal::runners
