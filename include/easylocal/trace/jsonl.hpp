#pragma once

#include <easylocal/trace/events.hpp>
#include <easylocal/trace/memory_recorder.hpp>
#include <easylocal/trace/tracer.hpp>
#include <easylocal/utils/detail/attributes.hpp>

#include <concepts>
#include <cstddef>
#include <ostream>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// JSONL serialization: streaming recorder and post-run memory_recorder output.
namespace easylocal::trace
{

template<class Writer, class Cost>
concept json_cost_writer_for = requires(
    Writer& writer,
    std::ostream& out,
    const Cost& cost) {
    writer(out, cost);
};

struct ostream_json_cost_writer
{
    template<class Cost>
    void operator()(std::ostream& out, const Cost& cost) const
        requires requires { out << cost; }
    {
        out << cost;
    }
};

namespace detail
{

inline void write_route_json(
    std::ostream& out,
    const std::vector<std::size_t>& route)
{
    out << '[';
    for (std::size_t index = 0; index < route.size(); ++index)
    {
        if (index != 0)
        {
            out << ',';
        }
        out << route[index];
    }
    out << ']';
}

inline void write_route_json_elements(
    std::ostream& out,
    const neighborhood_route_node* node,
    bool& first)
{
    if (node == nullptr)
    {
        return;
    }
    write_route_json_elements(out, node->parent, first);
    if (!first)
    {
        out << ',';
    }
    out << node->child;
    first = false;
}

inline void write_route_json(
    std::ostream& out,
    const neighborhood_route_node* node)
{
    out << '[';
    bool first = true;
    write_route_json_elements(out, node, first);
    out << ']';
}

} // namespace detail

template<class Cost, class CostWriter = ostream_json_cost_writer>
class jsonl_recorder
{
    static_assert(
        json_cost_writer_for<CostWriter, Cost>,
        "jsonl_recorder requires a cost writer callable as writer(ostream, cost)");

public:
    explicit jsonl_recorder(std::ostream& out) noexcept
        requires std::default_initializable<CostWriter>
        : out_{out}
    {
    }

    jsonl_recorder(std::ostream& out, CostWriter cost_writer)
        noexcept(std::is_nothrow_move_constructible_v<CostWriter>)
        : out_{out},
          cost_writer_{std::move(cost_writer)}
    {
    }

    template<class Event>
    static constexpr bool observes = true;

    void emit(const event::run_started<Cost>& value)
    {
        out_ << "{\"event\":\"run_started\",\"cost\":";
        cost_writer_(out_, value.cost);
        out_ << "}\n";
    }

    void emit(const event::move_evaluated<Cost>& value)
    {
        out_ << "{\"event\":\"move_evaluated\",\"evaluations\":" << value.evaluations
             << ",\"iterations\":" << value.iterations
             << ",\"current_cost\":";
        cost_writer_(out_, value.current_cost);
        out_ << ",\"candidate_cost\":";
        cost_writer_(out_, value.candidate_cost);
        out_ << ",\"neighborhood\":";
        detail::write_route_json(out_, value.neighborhood);
        out_ << "}\n";
    }

    void emit(const event::move_accepted<Cost>& value)
    {
        out_ << "{\"event\":\"move_accepted\",\"evaluations\":" << value.evaluations
             << ",\"iterations\":" << value.iterations
             << ",\"previous_cost\":";
        cost_writer_(out_, value.previous_cost);
        out_ << ",\"cost\":";
        cost_writer_(out_, value.cost);
        out_ << ",\"neighborhood\":";
        detail::write_route_json(out_, value.neighborhood);
        out_ << "}\n";
    }

    void emit(const event::incumbent_updated<Cost>& value)
    {
        out_ << "{\"event\":\"incumbent_updated\",\"evaluations\":" << value.evaluations
             << ",\"iterations\":" << value.iterations
             << ",\"previous_cost\":";
        cost_writer_(out_, value.previous_cost);
        out_ << ",\"cost\":";
        cost_writer_(out_, value.cost);
        out_ << "}\n";
    }

    void emit(const event::local_optimum<Cost>& value)
    {
        out_ << "{\"event\":\"local_optimum\",\"evaluations\":" << value.evaluations
             << ",\"iterations\":" << value.iterations
             << ",\"cost\":";
        cost_writer_(out_, value.cost);
        out_ << "}\n";
    }

    void emit(const event::neighborhood_selection& value)
    {
        out_ << "{\"event\":\"neighborhood_selection\",\"attempt\":" << value.attempt
             << ",\"child\":" << value.child
             << ",\"bias\":" << value.bias
             << ",\"active_bias_total\":" << value.active_bias_total
             << ",\"conditional_probability\":" << value.conditional_probability
             << ",\"produced_move\":" << (value.produced_move ? "true" : "false")
             << ",\"neighborhood\":";
        detail::write_route_json(out_, value.neighborhood);
        out_ << "}\n";
    }

    void emit(const event::solution_visited<Cost>& value)
    {
        out_ << "{\"event\":\"solution_visited\",\"evaluations\":" << value.evaluations
             << ",\"iterations\":" << value.iterations << ",\"hash\":" << value.hash
             << ",\"cost\":";
        cost_writer_(out_, value.cost);
        out_ << "}\n";
    }

    void emit(const event::aspiration_applied<Cost>& value)
    {
        out_ << "{\"event\":\"aspiration_applied\",\"evaluations\":" << value.evaluations
             << ",\"iterations\":" << value.iterations << ",\"cost\":";
        cost_writer_(out_, value.cost);
        out_ << "}\n";
    }

    void emit(const event::tabu_escape& value)
    {
        out_ << "{\"event\":\"tabu_escape\",\"evaluations\":" << value.evaluations
             << ",\"iterations\":" << value.iterations << ",\"moves\":" << value.moves
             << "}\n";
    }

    void emit(const event::run_finished<Cost>& value)
    {
        out_ << "{\"event\":\"run_finished\",\"evaluations\":" << value.evaluations
             << ",\"iterations\":" << value.iterations
             << ",\"cost\":";
        cost_writer_(out_, value.cost);
        out_ << "}\n";
    }

    void flush()
    {
        out_.flush();
    }

    [[nodiscard]]
    bool good() const
    {
        return out_.good();
    }

private:
    std::ostream& out_;
    EASYLOCAL_NO_UNIQUE_ADDRESS CostWriter cost_writer_{};
};

template<class Cost, class CostWriter>
    requires json_cost_writer_for<CostWriter, Cost>
void write_jsonl(
    std::ostream& out,
    const memory_recorder<Cost>& recorder,
    CostWriter cost_writer)
{
    using recorder_type = memory_recorder<Cost>;

    for (const auto& entry : recorder.records())
    {
        std::visit(
            [&](const auto& record) {
                using record_type = std::remove_cvref_t<decltype(record)>;
                if constexpr (std::same_as<record_type, typename recorder_type::run_started_record>)
                {
                    out << "{\"event\":\"run_started\",\"cost\":";
                    cost_writer(out, record.cost);
                    out << '}';
                }
                else if constexpr (std::same_as<record_type, typename recorder_type::move_evaluated_record>)
                {
                    out << "{\"event\":\"move_evaluated\",\"evaluations\":" << record.evaluations
                        << ",\"iterations\":" << record.iterations
                        << ",\"current_cost\":";
                    cost_writer(out, record.current_cost);
                    out << ",\"candidate_cost\":";
                    cost_writer(out, record.candidate_cost);
                    out << ",\"neighborhood\":";
                    detail::write_route_json(out, record.neighborhood);
                    out << '}';
                }
                else if constexpr (std::same_as<record_type, typename recorder_type::move_accepted_record>)
                {
                    out << "{\"event\":\"move_accepted\",\"evaluations\":" << record.evaluations
                        << ",\"iterations\":" << record.iterations
                        << ",\"previous_cost\":";
                    cost_writer(out, record.previous_cost);
                    out << ",\"cost\":";
                    cost_writer(out, record.cost);
                    out << ",\"neighborhood\":";
                    detail::write_route_json(out, record.neighborhood);
                    out << '}';
                }
                else if constexpr (std::same_as<record_type, typename recorder_type::incumbent_updated_record>)
                {
                    out << "{\"event\":\"incumbent_updated\",\"evaluations\":" << record.evaluations
                        << ",\"iterations\":" << record.iterations
                        << ",\"previous_cost\":";
                    cost_writer(out, record.previous_cost);
                    out << ",\"cost\":";
                    cost_writer(out, record.cost);
                    out << '}';
                }
                else if constexpr (std::same_as<record_type, typename recorder_type::local_optimum_record>)
                {
                    out << "{\"event\":\"local_optimum\",\"evaluations\":" << record.evaluations
                        << ",\"iterations\":" << record.iterations
                        << ",\"cost\":";
                    cost_writer(out, record.cost);
                    out << '}';
                }
                else if constexpr (std::same_as<record_type, typename recorder_type::neighborhood_selection_record>)
                {
                    out << "{\"event\":\"neighborhood_selection\",\"attempt\":" << record.attempt
                        << ",\"child\":" << record.child
                        << ",\"bias\":" << record.bias
                        << ",\"active_bias_total\":" << record.active_bias_total
                        << ",\"conditional_probability\":" << record.conditional_probability
                        << ",\"produced_move\":" << (record.produced_move ? "true" : "false")
                        << ",\"neighborhood\":";
                    detail::write_route_json(out, record.neighborhood);
                    out << '}';
                }
                else if constexpr (std::same_as<
                                       record_type,
                                       typename recorder_type::solution_visited_record>)
                {
                    out << "{\"event\":\"solution_visited\",\"evaluations\":"
                        << record.evaluations << ",\"iterations\":" << record.iterations
                        << ",\"hash\":" << record.hash << ",\"cost\":";
                    cost_writer(out, record.cost);
                    out << '}';
                }
                else if constexpr (std::same_as<
                                       record_type,
                                       typename recorder_type::aspiration_applied_record>)
                {
                    out << "{\"event\":\"aspiration_applied\",\"evaluations\":"
                        << record.evaluations << ",\"iterations\":" << record.iterations
                        << ",\"cost\":";
                    cost_writer(out, record.cost);
                    out << '}';
                }
                else if constexpr (
                    std::same_as<record_type, typename recorder_type::tabu_escape_record>)
                {
                    out << "{\"event\":\"tabu_escape\",\"evaluations\":"
                        << record.evaluations << ",\"iterations\":" << record.iterations
                        << ",\"moves\":" << record.moves << '}';
                }
                else if constexpr (std::same_as<record_type, typename recorder_type::run_finished_record>)
                {
                    out << "{\"event\":\"run_finished\",\"evaluations\":" << record.evaluations
                        << ",\"iterations\":" << record.iterations
                        << ",\"cost\":";
                    cost_writer(out, record.cost);
                    out << '}';
                }
                out << '\n';
            },
            entry);
    }
}

template<class Cost>
    requires json_cost_writer_for<ostream_json_cost_writer, Cost>
void write_jsonl(
    std::ostream& out,
    const memory_recorder<Cost>& recorder)
{
    write_jsonl(out, recorder, ostream_json_cost_writer{});
}

} // namespace easylocal::trace
