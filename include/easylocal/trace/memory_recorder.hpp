#pragma once

/// \file
/// Owning in-memory recorder for tests, short traces and in-process analysis.

#include <easylocal/trace/events.hpp>
#include <easylocal/trace/tracer.hpp>

#include <concepts>
#include <cstddef>
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
    /// A recorded `event::run_started`, as it is.
    using run_started_record = event::run_started<Cost>;

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

    /// A recorded `event::incumbent_updated`, as it is.
    using incumbent_updated_record = event::incumbent_updated<Cost>;

    /// A recorded `event::local_optimum`, as it is.
    using local_optimum_record = event::local_optimum<Cost>;

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

    /// A recorded `event::solution_visited`, as it is.
    using solution_visited_record = event::solution_visited<Cost>;

    /// A recorded `event::aspiration_applied`, as it is.
    using aspiration_applied_record = event::aspiration_applied<Cost>;

    /// A recorded `event::tabu_escape`, as it is.
    using tabu_escape_record = event::tabu_escape;

    /// A recorded `event::tabu_tenure_changed`, as it is.
    using tabu_tenure_changed_record = event::tabu_tenure_changed;

    /// A recorded `event::run_finished`, as it is.
    using run_finished_record = event::run_finished<Cost>;

    /// Whether an event is recorded as it is: it has no route to copy.
    template<class Event>
    static constexpr bool stored_as_is = std::same_as<Event, run_started_record>
        || std::same_as<Event, incumbent_updated_record>
        || std::same_as<Event, local_optimum_record>
        || std::same_as<Event, solution_visited_record>
        || std::same_as<Event, aspiration_applied_record>
        || std::same_as<Event, tabu_escape_record>
        || std::same_as<Event, tabu_tenure_changed_record>
        || std::same_as<Event, run_finished_record>;

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

    /// Records the event, an event without a route, as it is.
    template<class Event>
        requires stored_as_is<Event>
    void emit(const Event& value)
    {
        records_.emplace_back(value);
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

    template<class Tracer, class Event>
        requires stored_as_is<Event>
    static void replay_record(Tracer& tracer, const Event& value)
    {
        trace::emit(tracer, value);
    }

    std::vector<record> records_;
};

} // namespace easylocal::trace
