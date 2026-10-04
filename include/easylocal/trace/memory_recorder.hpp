#pragma once

/// \file
/// Owning in-memory recorder for tests, short traces and in-process analysis.

#include <easylocal/trace/events.hpp>
#include <easylocal/trace/tracer.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace easylocal::trace
{

/// A tracer that keeps a copy of every event in memory, in order, for tests,
/// short traces and in-process analysis.
///
/// It observes every event and copies the neighborhood routes.
template<class Cost>
class memory_recorder
{
public:
    /// A recorded `event::run_started`.
    struct run_started_record
    {
        /// The cost of the initial solution.
        Cost cost;
    };

    /// A recorded `event::move_evaluated`, its route copied.
    struct move_evaluated_record
    {
        /// Evaluations so far.
        std::size_t evaluations{};
        /// Iterations so far.
        std::size_t iterations{};
        /// The cost of the current solution.
        Cost current_cost;
        /// The cost the solution would have after the move.
        Cost candidate_cost;
        /// The route of the move through neighborhood unions, empty outside
        /// one.
        std::vector<std::size_t> neighborhood;
    };

    /// A recorded `event::move_accepted`, its route copied.
    struct move_accepted_record
    {
        /// Evaluations so far.
        std::size_t evaluations{};
        /// Iterations so far.
        std::size_t iterations{};
        /// The cost of the solution before the move.
        Cost previous_cost;
        /// The cost of the solution after the move.
        Cost cost;
        /// The route of the move through neighborhood unions, empty outside
        /// one.
        std::vector<std::size_t> neighborhood;
    };

    /// A recorded `event::incumbent_updated`.
    struct incumbent_updated_record
    {
        /// Evaluations so far.
        std::size_t evaluations{};
        /// Iterations so far.
        std::size_t iterations{};
        /// The cost of the previous best solution.
        Cost previous_cost;
        /// The cost of the new best solution.
        Cost cost;
    };

    /// A recorded `event::local_optimum`.
    struct local_optimum_record
    {
        /// Evaluations so far.
        std::size_t evaluations{};
        /// Iterations so far.
        std::size_t iterations{};
        /// The cost of the local optimum.
        Cost cost;
    };

    /// A recorded `event::neighborhood_selection`, its route copied.
    struct neighborhood_selection_record
    {
        /// The attempt within the draw, from 0.
        std::size_t attempt{};
        /// The index of the chosen child in its union.
        std::size_t child{};
        /// The bias of the chosen child.
        double bias{};
        /// The total bias of the children still active at this attempt.
        double active_bias_total{};
        /// The probability of choosing this child at this attempt.
        double conditional_probability{};
        /// Whether the child produced a move.
        bool produced_move{};
        /// The route of the chosen child through the enclosing unions.
        std::vector<std::size_t> neighborhood;
    };

    /// A recorded `event::solution_visited`.
    struct solution_visited_record
    {
        /// Evaluations so far.
        std::size_t evaluations{};
        /// Iterations so far.
        std::size_t iterations{};
        /// The solution hash of the solution.
        std::uint64_t hash{};
        /// The cost of the solution.
        Cost cost;
    };

    /// A recorded `event::aspiration_applied`.
    struct aspiration_applied_record
    {
        /// Evaluations so far.
        std::size_t evaluations{};
        /// Iterations so far.
        std::size_t iterations{};
        /// The cost of the solution after the move.
        Cost cost;
    };

    /// A recorded `event::tabu_escape`.
    struct tabu_escape_record
    {
        /// Evaluations so far.
        std::size_t evaluations{};
        /// Iterations so far.
        std::size_t iterations{};
        /// The number of random moves of the escape.
        std::size_t moves{};
    };

    /// A recorded `event::tabu_tenure_changed`.
    struct tabu_tenure_changed_record
    {
        /// Evaluations so far.
        std::size_t evaluations{};
        /// Iterations so far.
        std::size_t iterations{};
        /// The tenure before the change, 0 at the start of a run.
        std::size_t previous_tenure{};
        /// The new tenure.
        std::size_t tenure{};
    };

    /// A recorded `event::run_finished`.
    struct run_finished_record
    {
        /// Evaluations in the run.
        std::size_t evaluations{};
        /// Iterations in the run.
        std::size_t iterations{};
        /// The cost of the solution returned.
        Cost cost;
    };

    /// A recorded event.
    using record = std::variant<
        run_started_record,
        move_evaluated_record,
        move_accepted_record,
        incumbent_updated_record,
        local_optimum_record,
        neighborhood_selection_record,
        solution_visited_record,
        aspiration_applied_record,
        tabu_escape_record,
        tabu_tenure_changed_record,
        run_finished_record>;

    /// Whether the recorder receives Event: always.
    template<class Event>
    static constexpr bool observes = true;

    /// Records the event.
    void emit(const event::run_started<Cost>& value)
    {
        records_.emplace_back(run_started_record{value.cost});
    }

    /// Records the event.
    void emit(const event::move_evaluated<Cost>& value)
    {
        records_.emplace_back(move_evaluated_record{
            value.evaluations,
            value.iterations,
            value.current_cost,
            value.candidate_cost,
            copy_route(value.neighborhood),
        });
    }

    /// Records the event.
    void emit(const event::move_accepted<Cost>& value)
    {
        records_.emplace_back(move_accepted_record{
            value.evaluations,
            value.iterations,
            value.previous_cost,
            value.cost,
            copy_route(value.neighborhood),
        });
    }

    /// Records the event.
    void emit(const event::incumbent_updated<Cost>& value)
    {
        records_.emplace_back(incumbent_updated_record{
            value.evaluations,
            value.iterations,
            value.previous_cost,
            value.cost,
        });
    }

    /// Records the event.
    void emit(const event::local_optimum<Cost>& value)
    {
        records_.emplace_back(local_optimum_record{
            value.evaluations,
            value.iterations,
            value.cost,
        });
    }

    /// Records the event.
    void emit(const event::neighborhood_selection& value)
    {
        records_.emplace_back(neighborhood_selection_record{
            value.attempt,
            value.child,
            value.bias,
            value.active_bias_total,
            value.conditional_probability,
            value.produced_move,
            copy_route(value.neighborhood),
        });
    }

    /// Records the event.
    void emit(const event::solution_visited<Cost>& value)
    {
        records_.emplace_back(
            solution_visited_record{
                .evaluations = value.evaluations,
                .iterations = value.iterations,
                .hash = value.hash,
                .cost = value.cost,
            });
    }

    /// Records the event.
    void emit(const event::aspiration_applied<Cost>& value)
    {
        records_.emplace_back(
            aspiration_applied_record{
                .evaluations = value.evaluations,
                .iterations = value.iterations,
                .cost = value.cost,
            });
    }

    /// Records the event.
    void emit(const event::tabu_escape& value)
    {
        records_.emplace_back(
            tabu_escape_record{
                .evaluations = value.evaluations,
                .iterations = value.iterations,
                .moves = value.moves,
            });
    }

    /// Records the event.
    void emit(const event::tabu_tenure_changed& value)
    {
        records_.emplace_back(
            tabu_tenure_changed_record{
                .evaluations = value.evaluations,
                .iterations = value.iterations,
                .previous_tenure = value.previous_tenure,
                .tenure = value.tenure,
            });
    }

    /// Records the event.
    void emit(const event::run_finished<Cost>& value)
    {
        records_.emplace_back(run_finished_record{
            value.evaluations,
            value.iterations,
            value.cost,
        });
    }

    /// The recorded events, in the order they were emitted.
    [[nodiscard]]
    const std::vector<record>& records() const noexcept
    {
        return records_;
    }

    /// Emits the recorded events again, in order, to another tracer: a
    /// jsonl_recorder or a binary_recorder writes them after the run.
    template<class Tracer>
    void replay(Tracer& tracer) const
    {
        for (const auto& entry : records_)
            std::visit(
                [&tracer](const auto& value) { replay_record(tracer, value); },
                entry);
    }

private:
    // A route as the chain of nodes the events point to, from the root down.
    class route_nodes
    {
    public:
        explicit route_nodes(const std::vector<std::size_t>& route) : nodes_(route.size())
        {
            for (std::size_t index = 0; index < route.size(); ++index)
            {
                nodes_[index].child = route[index];
                nodes_[index].parent = index == 0 ? nullptr : &nodes_[index - 1];
            }
        }

        route_nodes(const route_nodes&) = delete;
        route_nodes& operator=(const route_nodes&) = delete;
        route_nodes(route_nodes&&) = delete;
        route_nodes& operator=(route_nodes&&) = delete;
        ~route_nodes() = default;

        [[nodiscard]]
        const neighborhood_route_node* leaf() const noexcept
        {
            return nodes_.empty() ? nullptr : &nodes_.back();
        }

    private:
        std::vector<neighborhood_route_node> nodes_;
    };

    template<class Tracer>
    static void replay_record(Tracer& tracer, const run_started_record& value)
    {
        trace::emit(tracer, event::run_started<Cost>{.cost = value.cost});
    }

    template<class Tracer>
    static void replay_record(Tracer& tracer, const move_evaluated_record& value)
    {
        const route_nodes route{value.neighborhood};
        trace::emit(
            tracer,
            event::move_evaluated<Cost>{
                .evaluations = value.evaluations,
                .iterations = value.iterations,
                .current_cost = value.current_cost,
                .candidate_cost = value.candidate_cost,
                .neighborhood = route.leaf(),
            });
    }

    template<class Tracer>
    static void replay_record(Tracer& tracer, const move_accepted_record& value)
    {
        const route_nodes route{value.neighborhood};
        trace::emit(
            tracer,
            event::move_accepted<Cost>{
                .evaluations = value.evaluations,
                .iterations = value.iterations,
                .previous_cost = value.previous_cost,
                .cost = value.cost,
                .neighborhood = route.leaf(),
            });
    }

    template<class Tracer>
    static void replay_record(Tracer& tracer, const incumbent_updated_record& value)
    {
        trace::emit(
            tracer,
            event::incumbent_updated<Cost>{
                .evaluations = value.evaluations,
                .iterations = value.iterations,
                .previous_cost = value.previous_cost,
                .cost = value.cost,
            });
    }

    template<class Tracer>
    static void replay_record(Tracer& tracer, const local_optimum_record& value)
    {
        trace::emit(
            tracer,
            event::local_optimum<Cost>{
                .evaluations = value.evaluations,
                .iterations = value.iterations,
                .cost = value.cost,
            });
    }

    template<class Tracer>
    static void replay_record(Tracer& tracer, const neighborhood_selection_record& value)
    {
        const route_nodes route{value.neighborhood};
        trace::emit(
            tracer,
            event::neighborhood_selection{
                .attempt = value.attempt,
                .child = value.child,
                .bias = value.bias,
                .active_bias_total = value.active_bias_total,
                .conditional_probability = value.conditional_probability,
                .produced_move = value.produced_move,
                .neighborhood = route.leaf(),
            });
    }

    template<class Tracer>
    static void replay_record(Tracer& tracer, const solution_visited_record& value)
    {
        trace::emit(
            tracer,
            event::solution_visited<Cost>{
                .evaluations = value.evaluations,
                .iterations = value.iterations,
                .hash = value.hash,
                .cost = value.cost,
            });
    }

    template<class Tracer>
    static void replay_record(Tracer& tracer, const aspiration_applied_record& value)
    {
        trace::emit(
            tracer,
            event::aspiration_applied<Cost>{
                .evaluations = value.evaluations,
                .iterations = value.iterations,
                .cost = value.cost,
            });
    }

    template<class Tracer>
    static void replay_record(Tracer& tracer, const tabu_escape_record& value)
    {
        trace::emit(
            tracer,
            event::tabu_escape{
                .evaluations = value.evaluations,
                .iterations = value.iterations,
                .moves = value.moves,
            });
    }

    template<class Tracer>
    static void replay_record(Tracer& tracer, const tabu_tenure_changed_record& value)
    {
        trace::emit(
            tracer,
            event::tabu_tenure_changed{
                .evaluations = value.evaluations,
                .iterations = value.iterations,
                .previous_tenure = value.previous_tenure,
                .tenure = value.tenure,
            });
    }

    template<class Tracer>
    static void replay_record(Tracer& tracer, const run_finished_record& value)
    {
        trace::emit(
            tracer,
            event::run_finished<Cost>{
                .evaluations = value.evaluations,
                .iterations = value.iterations,
                .cost = value.cost,
            });
    }

    std::vector<record> records_;
};

} // namespace easylocal::trace
