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

template<class Cost>
class memory_recorder
{
public:
    struct run_started_record
    {
        Cost cost;
    };

    struct move_evaluated_record
    {
        std::size_t evaluations{};
        std::size_t iterations{};
        Cost current_cost;
        Cost candidate_cost;
        std::vector<std::size_t> neighborhood;
    };

    struct move_accepted_record
    {
        std::size_t evaluations{};
        std::size_t iterations{};
        Cost previous_cost;
        Cost cost;
        std::vector<std::size_t> neighborhood;
    };

    struct incumbent_updated_record
    {
        std::size_t evaluations{};
        std::size_t iterations{};
        Cost previous_cost;
        Cost cost;
    };

    struct local_optimum_record
    {
        std::size_t evaluations{};
        std::size_t iterations{};
        Cost cost;
    };

    struct neighborhood_selection_record
    {
        std::size_t attempt{};
        std::size_t child{};
        double bias{};
        double active_bias_total{};
        double conditional_probability{};
        bool produced_move{};
        std::vector<std::size_t> neighborhood;
    };

    struct solution_visited_record
    {
        std::size_t evaluations{};
        std::size_t iterations{};
        std::uint64_t hash{};
        Cost cost;
    };

    struct aspiration_applied_record
    {
        std::size_t evaluations{};
        std::size_t iterations{};
        Cost cost;
    };

    struct tabu_escape_record
    {
        std::size_t evaluations{};
        std::size_t iterations{};
        std::size_t moves{};
    };

    struct tabu_tenure_changed_record
    {
        std::size_t evaluations{};
        std::size_t iterations{};
        std::size_t previous_tenure{};
        std::size_t tenure{};
    };

    struct run_finished_record
    {
        std::size_t evaluations{};
        std::size_t iterations{};
        Cost cost;
    };

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

    template<class Event>
    static constexpr bool observes = true;

    void emit(const event::run_started<Cost>& value)
    {
        records_.emplace_back(run_started_record{value.cost});
    }

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

    void emit(const event::incumbent_updated<Cost>& value)
    {
        records_.emplace_back(incumbent_updated_record{
            value.evaluations,
            value.iterations,
            value.previous_cost,
            value.cost,
        });
    }

    void emit(const event::local_optimum<Cost>& value)
    {
        records_.emplace_back(local_optimum_record{
            value.evaluations,
            value.iterations,
            value.cost,
        });
    }

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

    void emit(const event::aspiration_applied<Cost>& value)
    {
        records_.emplace_back(
            aspiration_applied_record{
                .evaluations = value.evaluations,
                .iterations = value.iterations,
                .cost = value.cost,
            });
    }

    void emit(const event::tabu_escape& value)
    {
        records_.emplace_back(
            tabu_escape_record{
                .evaluations = value.evaluations,
                .iterations = value.iterations,
                .moves = value.moves,
            });
    }

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

    void emit(const event::run_finished<Cost>& value)
    {
        records_.emplace_back(run_finished_record{
            value.evaluations,
            value.iterations,
            value.cost,
        });
    }

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
