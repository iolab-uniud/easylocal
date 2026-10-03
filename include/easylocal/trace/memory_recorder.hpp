#pragma once

#include <easylocal/trace/events.hpp>
#include <easylocal/trace/tracer.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// Owning in-memory recorder for tests, short traces and in-process analysis.
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

private:
    std::vector<record> records_;
};

} // namespace easylocal::trace
