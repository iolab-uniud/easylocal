#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <ostream>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace easylocal::trace
{

struct neighborhood_route_node
{
    std::size_t child{};
    const neighborhood_route_node* parent{};
};

inline auto copy_route(const neighborhood_route_node* node)
    -> std::vector<std::size_t>
{
    std::vector<std::size_t> route;
    for (auto* current = node; current != nullptr; current = current->parent)
    {
        route.push_back(current->child);
    }
    std::ranges::reverse(route);
    return route;
}

namespace event
{

template<class Cost>
struct run_started
{
    Cost cost;
};

template<class Cost>
struct move_evaluated
{
    std::size_t evaluations{};
    std::size_t iterations{};
    Cost current_cost;
    Cost candidate_cost;
    const neighborhood_route_node* neighborhood{};
};

template<class Cost>
struct move_accepted
{
    std::size_t evaluations{};
    std::size_t iterations{};
    Cost previous_cost;
    Cost cost;
    const neighborhood_route_node* neighborhood{};
};

template<class Cost>
struct incumbent_updated
{
    std::size_t evaluations{};
    std::size_t iterations{};
    Cost previous_cost;
    Cost cost;
};

template<class Cost>
struct local_optimum
{
    std::size_t evaluations{};
    std::size_t iterations{};
    Cost cost;
};

struct neighborhood_selection
{
    std::size_t attempt{};
    std::size_t child{};
    double bias{};
    double active_bias_total{};
    double conditional_probability{};
    bool produced_move{};
    const neighborhood_route_node* neighborhood{};
};

template<class Cost>
struct run_finished
{
    std::size_t evaluations{};
    std::size_t iterations{};
    Cost cost;
};

} // namespace event

struct null_tracer
{
    template<class Event>
    static constexpr bool observes = false;

    template<class Event>
    constexpr void emit(const Event&) noexcept
    {
    }
};

template<class Tracer, class Event>
concept tracer_for = requires {
    { std::remove_cvref_t<Tracer>::template observes<Event> } ->
        std::convertible_to<bool>;
} && (
    !std::remove_cvref_t<Tracer>::template observes<Event> ||
    requires(Tracer& tracer, const Event& event) {
        tracer.emit(event);
    });

template<class Tracer, class Event>
concept observes = tracer_for<Tracer, Event> &&
    std::remove_cvref_t<Tracer>::template observes<Event>;

template<class Event, class Tracer>
constexpr void emit(Tracer& tracer, const Event& value)
{
    if constexpr (observes<Tracer, Event>)
    {
        tracer.emit(value);
    }
}

namespace detail
{

template<class T>
struct is_variant : std::false_type
{
};

template<class... Ts>
struct is_variant<std::variant<Ts...>> : std::true_type
{
};

template<class T>
inline constexpr bool is_variant_v = is_variant<std::remove_cvref_t<T>>::value;

template<class Move>
void build_move_route(
    const Move& move,
    const neighborhood_route_node* parent,
    auto&& callback)
{
    if constexpr (is_variant_v<Move>)
    {
        std::visit(
            [&](const auto& tagged) {
                using tagged_type = std::remove_cvref_t<decltype(tagged)>;
                if constexpr (requires { tagged_type::index; tagged.value; })
                {
                    const neighborhood_route_node node{
                        .child = tagged_type::index,
                        .parent = parent,
                    };
                    build_move_route(tagged.value, &node, callback);
                }
                else
                {
                    callback(parent);
                }
            },
            move);
    }
    else
    {
        callback(parent);
    }
}

} // namespace detail

template<class Move, class Callback>
void with_move_route(const Move& move, Callback&& callback)
{
    detail::build_move_route(move, nullptr, std::forward<Callback>(callback));
}

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

    void emit(const event::run_finished<Cost>& value)
    {
        records_.emplace_back(run_finished_record{
            value.evaluations,
            value.iterations,
            value.cost,
        });
    }

    [[nodiscard]]
    auto records() const noexcept -> const std::vector<record>&
    {
        return records_;
    }

private:
    std::vector<record> records_;
};

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
    auto good() const -> bool
    {
        return out_.good();
    }

private:
    std::ostream& out_;
    [[no_unique_address]] CostWriter cost_writer_{};
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
