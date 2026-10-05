#pragma once

/// \file
/// JSONL serialization: streaming recorder and post-run memory_recorder output.

#include <easylocal/trace/detail/cost_shape.hpp>
#include <easylocal/trace/events.hpp>
#include <easylocal/trace/memory_recorder.hpp>
#include <easylocal/trace/tracer.hpp>
#include <easylocal/utils/detail/attributes.hpp>
#include <easylocal/utils/detail/number_text.hpp>

#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <limits>
#include <locale>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::trace
{

/// A writer of costs as JSON: `writer(out, cost)` writes cost to the stream
/// out.
template<class Writer, class Cost>
concept json_cost_writer_for = requires(
    Writer& writer,
    std::ostream& out,
    const Cost& cost) {
    writer(out, cost);
};

namespace detail
{

// A number as JSON: the shortest text that reads back to it, and null for
// NaN and the infinities, which JSON has no numbers for.
template<class Number>
void write_json_number(std::ostream& out, const Number value)
{
    if constexpr (std::floating_point<Number>)
    {
        if (!std::isfinite(value))
        {
            out << "null";
            return;
        }
    }
    out << easylocal::detail::number_text(value);
}

// A string as JSON: quoted, with the quote, the backslash and the control
// characters escaped.
inline void write_json_string(std::ostream& out, const std::string_view text)
{
    static constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (const char character : text)
    {
        const auto code = static_cast<unsigned char>(character);
        if (character == '"' || character == '\\')
            out << '\\' << character;
        else if (code < 0x20U)
            out << "\\u00" << hex[code >> 4U] << hex[code & 0xfU];
        else
            out << character;
    }
    out << '"';
}

// A 64-bit solution hash as JSON: a string of 16 hexadecimal digits, which
// JavaScript and jq read without rounding it to a double.
inline void write_json_hash(std::ostream& out, const std::uint64_t hash)
{
    static constexpr char hex[] = "0123456789abcdef";
    char text[18];
    text[0] = '"';
    for (int digit = 0; digit < 16; ++digit)
        text[16 - digit] = hex[(hash >> (4 * digit)) & 0xfU];
    text[17] = '"';
    out.write(text, sizeof text);
}

// Whether a cost has a JSON encoding by its shape: a number, or levels and a
// hard and a soft part whose own parts have one.
template<class Cost>
constexpr bool json_encodable_cost() noexcept
{
    if constexpr (number_cost<Cost>)
        return true;
    else if constexpr (leveled_shape<Cost>)
        return []<std::size_t... Index>(std::index_sequence<Index...>) {
            return (json_encodable_cost<level_type<Cost, Index>>() && ...);
        }(std::make_index_sequence<Cost::levels>{});
    else if constexpr (hard_soft_shape<Cost>)
        return json_encodable_cost<hard_type<Cost>>()
            && json_encodable_cost<soft_type<Cost>>();
    else
        return false;
}

// A cost as JSON, by its shape: a number, an array of levels, an object with
// "hard" and "soft", nested as the types are, as eltr.py decodes ELTR costs.
template<class Cost>
void write_json_cost(std::ostream& out, const Cost& cost)
{
    if constexpr (number_cost<Cost>)
    {
        write_json_number(out, cost);
    }
    else if constexpr (leveled_shape<Cost>)
    {
        out << '[';
        [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            ((out << (Index == 0 ? "" : ","),
                 write_json_cost(out, cost.template get<Index>())),
                ...);
        }(std::make_index_sequence<Cost::levels>{});
        out << ']';
    }
    else
    {
        out << "{\"hard\":";
        write_json_cost(out, cost.hard());
        out << ",\"soft\":";
        write_json_cost(out, cost.soft());
        out << '}';
    }
}

} // namespace detail

/// The JSON cost writer of the JSONL recorder by default: a number as the
/// shortest text that reads back to it (null for NaN and the infinities), a
/// cost::lexicographic or a cost::pareto as the array of its levels, a
/// cost::hierarchical as `{"hard": ..., "soft": ...}`, nested as the types
/// are; the shape of the costs `eltr.py` decodes from ELTR.
///
/// Another cost needs a writer of its own: the JSONL recorder takes it.
struct default_json_cost_writer
{
    /// Writes cost to out.
    template<class Cost>
        requires(detail::json_encodable_cost<Cost>())
    void operator()(std::ostream& out, const Cost& cost) const
    {
        detail::write_json_cost(out, cost);
    }
};

namespace detail
{

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

// The line a jsonl_recorder formats, one per thread, reused by every event.
// It has the classic locale: a locale that groups digits (1,234) would make
// the numbers invalid JSON.
inline std::ostringstream& jsonl_line_buffer()
{
    thread_local std::ostringstream line = [] {
        std::ostringstream buffer;
        buffer.imbue(std::locale::classic());
        return buffer;
    }();
    return line;
}

} // namespace detail

/// The options of a jsonl_recorder: the metadata of its header line, and
/// whether its events carry a timestamp.
struct jsonl_options
{
    /// Key-value pairs written in the header line: the instance, the runner,
    /// the seed, the parameters, whatever tells the run apart.
    std::vector<std::pair<std::string, std::string>> metadata{};
    /// Whether each event line ends with `"elapsed_ns"`, the nanoseconds of
    /// the steady clock since the recorder was constructed (false by default:
    /// the clock is not read).
    ///
    /// For a streaming recorder: `write_jsonl` replays a memory_recorder after
    /// the run, so its timestamps would be those of the replay.
    bool timestamps{false};
};

/// A tracer that writes each event to a stream as it is emitted, one JSON
/// object per line with an `"event"` field naming it.
///
/// The first line, written at construction, is the header:
/// `{"event":"trace","version":1,"metadata":{...}}`, as `eltr.py` starts its
/// JSONL output. It observes the core events of its cost type, the events
/// without a cost included, but not those of a run on another cost type nor
/// the application events; costs are written by CostWriter, routes as arrays
/// of child indices. The stream is held by reference.
/// Requires a CostWriter callable as `writer(out, cost)`.
template<class Cost, class CostWriter = default_json_cost_writer>
class jsonl_recorder
{
    static_assert(
        json_cost_writer_for<CostWriter, Cost>,
        "no JSON encoding for this cost type: give the JSONL recorder a cost writer "
        "callable as writer(std::ostream&, const Cost&)");

public:
    /// The cost type of the events recorded.
    using cost_type = Cost;

    /// The version of the JSONL trace format written, in the header line.
    static constexpr unsigned format_version = 1;

    /// Writes to out, with a default-constructed cost writer, after the header
    /// line.
    explicit jsonl_recorder(std::ostream& out, const jsonl_options& options = {})
        requires std::default_initializable<CostWriter>
        : out_{out}
    {
        start(options);
    }

    /// Writes to out, costs with cost_writer, after the header line.
    jsonl_recorder(
        std::ostream& out,
        CostWriter cost_writer,
        const jsonl_options& options = {})
        : out_{out}, cost_writer_{std::move(cost_writer)}
    {
        start(options);
    }

    /// Whether the recorder receives Event: a core event without a cost or of
    /// its cost type.
    template<class Event>
    static constexpr bool observes = detail::core_event_of<Event, Cost>;

    /// Writes the event as a JSON line.
    void emit(const event::run_context& value)
    {
        write_line([&](std::ostream& out) {
            out << "{\"event\":\"run_context\",\"stage\":";
            detail::write_json_string(out, value.stage);
            out << ",\"stage_index\":" << value.stage_index
                << ",\"attempt\":" << value.attempt << "}\n";
        });
    }

    /// Writes the event as a JSON line.
    void emit(const event::run_started<Cost>& value)
    {
        write_line([&](std::ostream& out) {
            out << "{\"event\":\"run_started\",\"cost\":";
            cost_writer_(out, value.cost);
            out << "}\n";
        });
    }

    /// Writes the event as a JSON line.
    void emit(const event::move_evaluated<Cost>& value)
    {
        write_line([&](std::ostream& out) {
            out << "{\"event\":\"move_evaluated\",\"evaluations\":" << value.evaluations
                << ",\"iterations\":" << value.iterations << ",\"current_cost\":";
            cost_writer_(out, value.current_cost);
            out << ",\"candidate_cost\":";
            cost_writer_(out, value.candidate_cost);
            out << ",\"neighborhood\":";
            detail::write_route_json(out, value.neighborhood);
            out << "}\n";
        });
    }

    /// Writes the event as a JSON line.
    void emit(const event::move_accepted<Cost>& value)
    {
        write_line([&](std::ostream& out) {
            out << "{\"event\":\"move_accepted\",\"evaluations\":" << value.evaluations
                << ",\"iterations\":" << value.iterations << ",\"previous_cost\":";
            cost_writer_(out, value.previous_cost);
            out << ",\"cost\":";
            cost_writer_(out, value.cost);
            out << ",\"neighborhood\":";
            detail::write_route_json(out, value.neighborhood);
            out << "}\n";
        });
    }

    /// Writes the event as a JSON line.
    void emit(const event::incumbent_updated<Cost>& value)
    {
        write_line([&](std::ostream& out) {
            out << "{\"event\":\"incumbent_updated\",\"evaluations\":"
                << value.evaluations << ",\"iterations\":" << value.iterations
                << ",\"previous_cost\":";
            cost_writer_(out, value.previous_cost);
            out << ",\"cost\":";
            cost_writer_(out, value.cost);
            out << "}\n";
        });
    }

    /// Writes the event as a JSON line.
    void emit(const event::local_optimum<Cost>& value)
    {
        write_line([&](std::ostream& out) {
            out << "{\"event\":\"local_optimum\",\"evaluations\":" << value.evaluations
                << ",\"iterations\":" << value.iterations << ",\"cost\":";
            cost_writer_(out, value.cost);
            out << "}\n";
        });
    }

    /// Writes the event as a JSON line.
    void emit(const event::neighborhood_selection& value)
    {
        write_line([&](std::ostream& out) {
            out << "{\"event\":\"neighborhood_selection\",\"attempt\":" << value.attempt
                << ",\"child\":" << value.child << ",\"bias\":";
            detail::write_json_number(out, value.bias);
            out << ",\"active_bias_total\":";
            detail::write_json_number(out, value.active_bias_total);
            out << ",\"conditional_probability\":";
            detail::write_json_number(out, value.conditional_probability);
            out << ",\"produced_move\":" << (value.produced_move ? "true" : "false")
                << ",\"neighborhood\":";
            detail::write_route_json(out, value.neighborhood);
            out << "}\n";
        });
    }

    /// Writes the event as a JSON line.
    void emit(const event::solution_visited<Cost>& value)
    {
        write_line([&](std::ostream& out) {
            out << "{\"event\":\"solution_visited\",\"evaluations\":" << value.evaluations
                << ",\"iterations\":" << value.iterations << ",\"hash\":";
            detail::write_json_hash(out, value.hash);
            out << ",\"cost\":";
            cost_writer_(out, value.cost);
            out << ",\"previous_hash\":";
            detail::write_json_hash(out, value.previous_hash);
            out << "}\n";
        });
    }

    /// Writes the event as a JSON line.
    void emit(const event::aspiration_applied<Cost>& value)
    {
        write_line([&](std::ostream& out) {
            out << "{\"event\":\"aspiration_applied\",\"evaluations\":"
                << value.evaluations << ",\"iterations\":" << value.iterations
                << ",\"cost\":";
            cost_writer_(out, value.cost);
            out << "}\n";
        });
    }

    /// Writes the event as a JSON line.
    void emit(const event::tabu_escape& value)
    {
        write_line([&](std::ostream& out) {
            out << "{\"event\":\"tabu_escape\",\"evaluations\":" << value.evaluations
                << ",\"iterations\":" << value.iterations << ",\"moves\":" << value.moves
                << "}\n";
        });
    }

    /// Writes the event as a JSON line.
    void emit(const event::tabu_tenure_changed& value)
    {
        write_line([&](std::ostream& out) {
            out << "{\"event\":\"tabu_tenure_changed\",\"evaluations\":"
                << value.evaluations << ",\"iterations\":" << value.iterations
                << ",\"previous_tenure\":" << value.previous_tenure
                << ",\"tenure\":" << value.tenure << "}\n";
        });
    }

    /// Writes the event as a JSON line.
    void emit(const event::run_finished<Cost>& value)
    {
        write_line([&](std::ostream& out) {
            out << "{\"event\":\"run_finished\",\"evaluations\":" << value.evaluations
                << ",\"iterations\":" << value.iterations << ",\"cost\":";
            cost_writer_(out, value.cost);
            out << ",\"termination\":\"" << to_string(value.termination) << "\"}\n";
        });
    }

    /// Flushes the stream.
    void flush()
    {
        out_.flush();
    }

    /// Whether the stream has had no error.
    [[nodiscard]]
    bool good() const
    {
        return out_.good();
    }

private:
    // Writes the header line, then starts the clock of the timestamps.
    void start(const jsonl_options& options)
    {
        write_line(
            [&](std::ostream& out) {
                out << "{\"event\":\"trace\",\"version\":" << format_version
                    << ",\"metadata\":{";
                bool first = true;
                for (const auto& [key, value] : options.metadata)
                {
                    if (!first)
                        out << ',';
                    first = false;
                    detail::write_json_string(out, key);
                    out << ':';
                    detail::write_json_string(out, value);
                }
                out << "}}\n";
            },
            false);
        if (options.timestamps)
            start_ = std::chrono::steady_clock::now();
    }

    // Formats a line with the stream's flags and precision, but not its
    // locale, then writes it at once: a cost writer that throws leaves no part
    // of it in the stream. A stamped line gets the timestamp before its
    // closing brace.
    template<class Format>
    void write_line(Format&& format, const bool stamped = true)
    {
        std::optional<std::chrono::steady_clock::duration> elapsed;
        if (stamped && start_)
            elapsed = std::chrono::steady_clock::now() - *start_;
        auto& line = detail::jsonl_line_buffer();
        line.str({});
        line.clear();
        line.flags(out_.flags());
        line.precision(out_.precision());
        std::forward<Format>(format)(static_cast<std::ostream&>(line));
        if (elapsed)
        {
            // The line ends with "}\n": the field goes before them.
            line.seekp(-2, std::ios_base::end);
            line << ",\"elapsed_ns\":"
                 << easylocal::detail::number_text(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(*elapsed)
                            .count())
                 << "}\n";
        }
        const auto text = line.view();
        out_.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    std::ostream& out_;
    EASYLOCAL_NO_UNIQUE_ADDRESS CostWriter cost_writer_{};
    // When the recorder started, with timestamps.
    std::optional<std::chrono::steady_clock::time_point> start_;
};

/// Writes the events of a memory_recorder as JSONL, after the header line,
/// one line per event as jsonl_recorder writes them during a run.
template<class Cost, class CostWriter>
    requires json_cost_writer_for<CostWriter, Cost>
void write_jsonl(
    std::ostream& out,
    const memory_recorder<Cost>& recorder,
    CostWriter cost_writer,
    const jsonl_options& options = {})
{
    jsonl_recorder<Cost, CostWriter> json{out, std::move(cost_writer), options};
    recorder.replay(json);
}

/// Writes the events of a memory_recorder as JSONL, with the default cost
/// writer, after the header line.
template<class Cost>
    requires json_cost_writer_for<default_json_cost_writer, Cost>
void write_jsonl(
    std::ostream& out,
    const memory_recorder<Cost>& recorder,
    const jsonl_options& options = {})
{
    write_jsonl(out, recorder, default_json_cost_writer{}, options);
}

} // namespace easylocal::trace
