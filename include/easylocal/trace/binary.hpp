#pragma once

/// \file
/// ELTR binary recording: encoders, buffered and asynchronous recorders.
///
/// A trace describes itself: its header gives the metadata of the run, the
/// layout of the costs and the fields of every event (docs/tracing.md).

#include <easylocal/trace/detail/cost_shape.hpp>
#include <easylocal/trace/events.hpp>
#include <easylocal/trace/tracer.hpp>
#include <easylocal/utils/detail/attributes.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <ostream>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::trace
{

/// The types of the fields of an ELTR record.
enum class binary_type : std::uint8_t
{
    /// An unsigned 8-bit integer.
    u8 = 1,
    /// A signed 8-bit integer.
    i8 = 2,
    /// An unsigned 16-bit integer.
    u16 = 3,
    /// A signed 16-bit integer.
    i16 = 4,
    /// An unsigned 32-bit integer.
    u32 = 5,
    /// A signed 32-bit integer.
    i32 = 6,
    /// An unsigned 64-bit integer.
    u64 = 7,
    /// A signed 64-bit integer.
    i64 = 8,
    /// A 32-bit floating-point number.
    f32 = 9,
    /// A 64-bit floating-point number.
    f64 = 10,
    /// A boolean, one byte 0 or 1.
    boolean = 11,
    /// A string: a u32 size, then the UTF-8 bytes.
    string = 12,
    /// A byte sequence: a u32 size, then the bytes.
    bytes = 13,
    /// A neighborhood route: a u32 count, then a u32 child index per level.
    route = 14,
    /// A cost, written as the fields of the cost layout of the trace header.
    cost = 15,
};

/// A field of an ELTR record: its name and type.
struct binary_field
{
    /// The name of the field.
    std::string name;
    /// The type of the field.
    binary_type type;
};

/// The name and fields of the records of one tag.
struct binary_event_schema
{
    /// The name of the event.
    std::string name;
    /// The fields of a record, in order.
    std::vector<binary_field> fields;
};

/// Appends the fields of an ELTR record to a byte buffer, in their ELTR
/// representation: little-endian integers, IEEE floats, size-prefixed strings.
///
/// The buffer is held by reference; the recorder writes the tag and the size
/// of the record.
class binary_record_writer
{
public:
    /// Appends to buffer.
    explicit binary_record_writer(std::vector<char>& buffer) noexcept
        : buffer_{buffer}
    {
    }

    /// Writes value as a u8.
    void u8(const std::uint8_t value)
    {
        buffer_.push_back(static_cast<char>(value));
    }

    /// Writes value as an i8.
    void i8(const std::int8_t value)
    {
        u8(static_cast<std::uint8_t>(value));
    }

    /// Writes value as a byte, 1 for true and 0 for false.
    void boolean(const bool value)
    {
        u8(value ? 1U : 0U);
    }

    /// Writes value as a little-endian u16.
    void u16(const std::uint16_t value)
    {
        append_unsigned_le(value);
    }

    /// Writes value as a little-endian i16.
    void i16(const std::int16_t value)
    {
        append_unsigned_le(static_cast<std::uint16_t>(value));
    }

    /// Writes value as a little-endian u32.
    void u32(const std::uint32_t value)
    {
        append_unsigned_le(value);
    }

    /// Writes value as a little-endian i32.
    void i32(const std::int32_t value)
    {
        append_unsigned_le(static_cast<std::uint32_t>(value));
    }

    /// Writes value as a little-endian u64.
    void u64(const std::uint64_t value)
    {
        append_unsigned_le(value);
    }

    /// Writes value as a little-endian i64.
    void i64(const std::int64_t value)
    {
        u64(static_cast<std::uint64_t>(value));
    }

    /// Writes value as an f32, the little-endian bits of the float.
    void f32(const float value)
    {
        u32(std::bit_cast<std::uint32_t>(value));
    }

    /// Writes value as an f64, the little-endian bits of the double.
    void f64(const double value)
    {
        u64(std::bit_cast<std::uint64_t>(value));
    }

    /// A field list: u32 count, then the name and the type of each field.
    void fields(const std::span<const binary_field> value)
    {
        u32(static_cast<std::uint32_t>(value.size()));
        for (const auto& field : value)
        {
            string(field.name);
            u8(static_cast<std::uint8_t>(field.type));
        }
    }

    /// Writes an event schema: the described tag as a u8, the name, then the
    /// field list.
    void schema(const std::uint8_t tag, const binary_event_schema& value)
    {
        u8(tag);
        string(value.name);
        fields(value.fields);
    }

    /// Writes the size bytes at data as they are, with no size prefix.
    void raw_bytes(const void* data, const std::size_t size)
    {
        if (size == 0)
        {
            return;
        }
        const auto offset = buffer_.size();
        buffer_.resize(offset + size);
        std::memcpy(buffer_.data() + offset, data, size);
    }

    /// Writes value as bytes: a u32 size, then the bytes.
    void bytes(const std::span<const std::byte> value)
    {
        u32(static_cast<std::uint32_t>(value.size()));
        raw_bytes(value.data(), value.size());
    }

    /// Writes value as a string: a u32 size, then its bytes.
    void string(const std::string_view value)
    {
        u32(static_cast<std::uint32_t>(value.size()));
        raw_bytes(value.data(), value.size());
    }

    /// Writes the route ending at node: a u32 count, then the u32 child indices
    /// from the outermost union inward.
    void route(const neighborhood_route_node* node)
    {
        u32(static_cast<std::uint32_t>(route_size(node)));
        write_route(node);
    }

private:
    template<std::unsigned_integral Integer>
    void append_unsigned_le(Integer value)
    {
        if constexpr (std::endian::native == std::endian::big)
        {
            value = std::byteswap(value);
        }

        if constexpr (
            std::endian::native == std::endian::little ||
            std::endian::native == std::endian::big)
        {
            raw_bytes(&value, sizeof(value));
        }
        else
        {
            for (unsigned index = 0; index < sizeof(value); ++index)
            {
                u8(static_cast<std::uint8_t>(
                    (value >> (index * 8U)) & static_cast<Integer>(0xffU)));
            }
        }
    }

    [[nodiscard]]
    static std::size_t route_size(const neighborhood_route_node* node) noexcept
    {
        std::size_t result = 0;
        for (auto* current = node; current != nullptr; current = current->parent)
        {
            ++result;
        }
        return result;
    }

    void write_route(const neighborhood_route_node* node)
    {
        if (node == nullptr)
        {
            return;
        }
        write_route(node->parent);
        u32(static_cast<std::uint32_t>(node->child));
    }

    std::vector<char>& buffer_;
};

/// A cost writer writes a cost and describes what it writes: fields() gives
/// the fields in order, a scalar cost one field with an empty name.
template<class Writer, class Cost>
concept binary_cost_writer_for = requires(
    Writer& writer,
    binary_record_writer& out,
    const Cost& cost) {
    writer(out, cost);
    { std::as_const(writer).fields() } -> std::ranges::input_range;
    requires std::convertible_to<
        std::ranges::range_reference_t<decltype(std::as_const(writer).fields())>,
        const binary_field&>;
};

namespace detail
{

template<class Cost>
struct default_binary_cost : std::false_type
{
};

template<class Cost>
    requires std::is_arithmetic_v<Cost>
struct default_binary_cost<Cost> : std::true_type
{
    static constexpr binary_type type = std::floating_point<Cost>
        ? binary_type::f64
        : (std::signed_integral<Cost> ? binary_type::i64 : binary_type::u64);

    static void write(binary_record_writer& out, const Cost& cost)
    {
        if constexpr (std::floating_point<Cost>)
            out.f64(static_cast<double>(cost));
        else if constexpr (std::signed_integral<Cost>)
            out.i64(static_cast<std::int64_t>(cost));
        else
            out.u64(static_cast<std::uint64_t>(cost));
    }

    static void describe(std::vector<binary_field>& fields, const std::string& name)
    {
        fields.push_back({name, type});
    }
};

inline std::string field_path(const std::string& prefix, const std::string_view name)
{
    return prefix.empty() ? std::string{name} : prefix + "." + std::string{name};
}

// The cost types of easylocal::cost, by their shape (detail/cost_shape.hpp),
// when each of their parts has an encoding.
template<class Cost, std::size_t... Index>
consteval bool levels_encodable(std::index_sequence<Index...>)
{
    return (default_binary_cost<level_type<Cost, Index>>::value && ...);
}

template<class Cost>
concept leveled_cost = leveled_shape<Cost>
    && levels_encodable<Cost>(std::make_index_sequence<Cost::levels>{});

template<class Cost>
concept hard_soft_cost =
    hard_soft_shape<Cost> && default_binary_cost<hard_type<Cost>>::value
    && default_binary_cost<soft_type<Cost>>::value;

template<leveled_cost Cost>
struct default_binary_cost<Cost> : std::true_type
{
    static void write(binary_record_writer& out, const Cost& cost)
    {
        [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            (default_binary_cost<level_type<Cost, Index>>::write(
                 out,
                 cost.template get<Index>()),
                ...);
        }(std::make_index_sequence<Cost::levels>{});
    }

    static void describe(std::vector<binary_field>& fields, const std::string& name)
    {
        [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            (default_binary_cost<level_type<Cost, Index>>::describe(
                 fields,
                 field_path(name, std::to_string(Index))),
                ...);
        }(std::make_index_sequence<Cost::levels>{});
    }
};

template<hard_soft_cost Cost>
struct default_binary_cost<Cost> : std::true_type
{
    static void write(binary_record_writer& out, const Cost& cost)
    {
        default_binary_cost<hard_type<Cost>>::write(out, cost.hard());
        default_binary_cost<soft_type<Cost>>::write(out, cost.soft());
    }

    static void describe(std::vector<binary_field>& fields, const std::string& name)
    {
        default_binary_cost<hard_type<Cost>>::describe(fields, field_path(name, "hard"));
        default_binary_cost<soft_type<Cost>>::describe(fields, field_path(name, "soft"));
    }
};

} // namespace detail

/// The cost writer of the recorders by default: an arithmetic cost as i64, u64
/// or f64; a cost::lexicographic or a cost::pareto as its levels ("0", "1",
/// ...) and a cost::hierarchical as "hard" and "soft", nested as the types are.
template<class Cost>
struct default_binary_cost_writer
{
    static_assert(
        detail::default_binary_cost<Cost>::value,
        "no default ELTR encoding for this cost type: give the recorder a cost writer "
        "with operator()(binary_record_writer&, const Cost&) and fields()");

    /// Writes cost to out.
    void operator()(binary_record_writer& out, const Cost& cost) const
    {
        detail::default_binary_cost<Cost>::write(out, cost);
    }

    /// The fields the writer writes for a cost, in order.
    [[nodiscard]]
    std::vector<binary_field> fields() const
    {
        std::vector<binary_field> result;
        detail::default_binary_cost<Cost>::describe(result, {});
        return result;
    }
};

/// An application event may describe its records with an ADL function
/// describe_binary_event(std::type_identity<Event>) returning its
/// binary_event_schema; the recorder then writes
/// the schema before the first record of its tag, and decoders name its fields.
template<class Event>
concept described_binary_event = requires {
    {
        describe_binary_event(std::type_identity<Event>{})
    } -> std::convertible_to<binary_event_schema>;
};

/// The ELTR tag of the application event of index Index, 0 to 127: tag
/// 128 + Index.
///
/// Tags 1 to 127 are reserved to EasyLocal.
template<std::uint8_t Index>
consteval std::uint8_t user_binary_event_tag()
{
    static_assert(Index < 128, "EasyLocal user binary event tags have indices 0..127");
    return static_cast<std::uint8_t>(128U + Index);
}

/// The options of a binary recorder: its buffering and the metadata of its
/// header.
struct binary_buffer_options
{
    /// The bytes buffered before a write (256 KiB by default, at least 1).
    std::size_t block_size = 256U * 1024U;
    /// The blocks an async recorder can queue to its writer thread before the
    /// search waits (at least 1).
    ///
    /// The recorder allocates them, and one more, at construction: the queue
    /// is finite, and an unlimited one throws `std::invalid_argument`.
    std::size_t async_queue_blocks = 4;
    /// Key-value pairs written in the header: the instance, the runner, the
    /// seed, the parameters, whatever tells the run apart.
    std::vector<std::pair<std::string, std::string>> metadata{};
    /// Whether each core event ends with an `elapsed_ns` field (`u64`), the
    /// nanoseconds of the steady clock since the recorder was constructed
    /// (false by default: the clock is not read). Application events carry
    /// none.
    bool timestamps{false};
};

namespace detail
{

enum class core_binary_event_tag : std::uint8_t
{
    // The schema of an application event's records (tag u8, name, fields).
    schema = 0,
    run_started = 1,
    move_evaluated = 2,
    move_accepted = 3,
    incumbent_updated = 4,
    local_optimum = 5,
    neighborhood_selection = 6,
    run_finished = 7,
    solution_visited = 8,
    aspiration_applied = 9,
    tabu_escape = 10,
    tabu_tenure_changed = 11,
    run_context = 12,
};

inline void finish_record(std::vector<char>& buffer, std::size_t payload_offset);

inline void append_u32_le(std::vector<char>& buffer, const std::uint32_t value)
{
    binary_record_writer out{buffer};
    out.u32(value);
}

// The schemas of the core events, by tag; each matches its encode_core_event,
// followed by elapsed_ns with timestamps.
inline std::vector<std::pair<std::uint8_t, binary_event_schema>> core_event_schemas(
    const bool timestamps)
{
    using enum binary_type;
    const binary_field evaluations{"evaluations", u64};
    const binary_field iterations{"iterations", u64};
    const auto tag = [](const core_binary_event_tag value) {
        return static_cast<std::uint8_t>(value);
    };
    using enum core_binary_event_tag;
    std::vector<std::pair<std::uint8_t, binary_event_schema>> schemas{
        {tag(run_started), {"run_started", {{"cost", cost}}}},
        {tag(move_evaluated),
            {"move_evaluated",
                {evaluations,
                    iterations,
                    {"current_cost", cost},
                    {"candidate_cost", cost},
                    {"neighborhood", route}}}},
        {tag(move_accepted),
            {"move_accepted",
                {evaluations,
                    iterations,
                    {"previous_cost", cost},
                    {"cost", cost},
                    {"neighborhood", route}}}},
        {tag(incumbent_updated),
            {"incumbent_updated",
                {evaluations, iterations, {"previous_cost", cost}, {"cost", cost}}}},
        {tag(local_optimum),
            {"local_optimum", {evaluations, iterations, {"cost", cost}}}},
        {tag(neighborhood_selection),
            {"neighborhood_selection",
                {{"attempt", u64},
                    {"child", u64},
                    {"bias", f64},
                    {"active_bias_total", f64},
                    {"conditional_probability", f64},
                    {"produced_move", boolean},
                    {"neighborhood", route}}}},
        {tag(run_finished),
            {"run_finished",
                {evaluations, iterations, {"cost", cost}, {"termination", string}}}},
        {tag(solution_visited),
            {"solution_visited",
                {evaluations,
                    iterations,
                    {"hash", u64},
                    {"cost", cost},
                    {"previous_hash", u64}}}},
        {tag(aspiration_applied),
            {"aspiration_applied", {evaluations, iterations, {"cost", cost}}}},
        {tag(tabu_escape), {"tabu_escape", {evaluations, iterations, {"moves", u64}}}},
        {tag(tabu_tenure_changed),
            {"tabu_tenure_changed",
                {evaluations, iterations, {"previous_tenure", u64}, {"tenure", u64}}}},
        {tag(run_context),
            {"run_context", {{"stage", string}, {"stage_index", u64}, {"attempt", u64}}}},
    };
    if (timestamps)
        for (auto& [_, schema] : schemas)
            schema.fields.push_back({"elapsed_ns", u64});
    return schemas;
}

// The version of the ELTR format, in the header after the magic.
inline constexpr std::uint32_t eltr_format_version = 1;

// "ELTR", the format version, then the header: its size (u32), the metadata
// (u32 count, key and value strings), the cost layout (a field list) and the
// core event schemas (u32 count, then tag, name and field list each).
inline void append_trace_header(
    std::vector<char>& buffer,
    const std::span<const binary_field> cost_fields,
    const std::span<const std::pair<std::string, std::string>> metadata,
    const bool timestamps)
{
    static constexpr char magic[] = {'E', 'L', 'T', 'R'};
    buffer.insert(buffer.end(), std::begin(magic), std::end(magic));
    append_u32_le(buffer, eltr_format_version);

    const auto size_offset = buffer.size();
    append_u32_le(buffer, 0U);
    binary_record_writer out{buffer};
    out.u32(static_cast<std::uint32_t>(metadata.size()));
    for (const auto& [key, value] : metadata)
    {
        out.string(key);
        out.string(value);
    }
    out.fields(cost_fields);
    const auto schemas = core_event_schemas(timestamps);
    out.u32(static_cast<std::uint32_t>(schemas.size()));
    for (const auto& [tag, schema] : schemas)
        out.schema(tag, schema);
    finish_record(buffer, size_offset + 4U);
}

inline std::size_t begin_record(std::vector<char>& buffer, const std::uint8_t tag)
{
    binary_record_writer out{buffer};
    out.u8(tag);
    const auto size_offset = buffer.size();
    out.u32(0U);
    return size_offset + 4U;
}

inline void finish_record(
    std::vector<char>& buffer,
    const std::size_t payload_offset)
{
    const auto payload_size = buffer.size() - payload_offset;
    const auto size_offset = payload_offset - 4U;
    const auto size = static_cast<std::uint32_t>(payload_size);
    for (unsigned index = 0; index < 4; ++index)
    {
        buffer[size_offset + index] =
            static_cast<char>((size >> (index * 8)) & 0xffU);
    }
}

template<class Cost>
constexpr std::uint8_t core_event_tag(const event::run_started<Cost>&) noexcept
{
    return static_cast<std::uint8_t>(core_binary_event_tag::run_started);
}

template<class Cost>
constexpr std::uint8_t core_event_tag(const event::move_evaluated<Cost>&) noexcept
{
    return static_cast<std::uint8_t>(core_binary_event_tag::move_evaluated);
}

template<class Cost>
constexpr std::uint8_t core_event_tag(const event::move_accepted<Cost>&) noexcept
{
    return static_cast<std::uint8_t>(core_binary_event_tag::move_accepted);
}

template<class Cost>
constexpr std::uint8_t core_event_tag(const event::incumbent_updated<Cost>&) noexcept
{
    return static_cast<std::uint8_t>(core_binary_event_tag::incumbent_updated);
}

template<class Cost>
constexpr std::uint8_t core_event_tag(const event::local_optimum<Cost>&) noexcept
{
    return static_cast<std::uint8_t>(core_binary_event_tag::local_optimum);
}

constexpr std::uint8_t core_event_tag(const event::neighborhood_selection&) noexcept
{
    return static_cast<std::uint8_t>(core_binary_event_tag::neighborhood_selection);
}

template<class Cost>
constexpr std::uint8_t core_event_tag(const event::run_finished<Cost>&) noexcept
{
    return static_cast<std::uint8_t>(core_binary_event_tag::run_finished);
}

template<class Cost>
constexpr std::uint8_t core_event_tag(const event::solution_visited<Cost>&) noexcept
{
    return static_cast<std::uint8_t>(core_binary_event_tag::solution_visited);
}

template<class Cost>
constexpr std::uint8_t core_event_tag(const event::aspiration_applied<Cost>&) noexcept
{
    return static_cast<std::uint8_t>(core_binary_event_tag::aspiration_applied);
}

constexpr std::uint8_t core_event_tag(const event::tabu_escape&) noexcept
{
    return static_cast<std::uint8_t>(core_binary_event_tag::tabu_escape);
}

template<class Cost, class CostWriter>
    requires binary_cost_writer_for<CostWriter, Cost>
void encode_core_event(
    binary_record_writer& out,
    const event::solution_visited<Cost>& value,
    CostWriter& cost_writer)
{
    out.u64(value.evaluations);
    out.u64(value.iterations);
    out.u64(value.hash);
    cost_writer(out, value.cost);
    out.u64(value.previous_hash);
}

template<class Cost, class CostWriter>
    requires binary_cost_writer_for<CostWriter, Cost>
void encode_core_event(
    binary_record_writer& out,
    const event::aspiration_applied<Cost>& value,
    CostWriter& cost_writer)
{
    out.u64(value.evaluations);
    out.u64(value.iterations);
    cost_writer(out, value.cost);
}

template<class CostWriter>
void encode_core_event(
    binary_record_writer& out,
    const event::tabu_escape& value,
    CostWriter&)
{
    out.u64(value.evaluations);
    out.u64(value.iterations);
    out.u64(value.moves);
}

constexpr std::uint8_t core_event_tag(const event::tabu_tenure_changed&) noexcept
{
    return static_cast<std::uint8_t>(core_binary_event_tag::tabu_tenure_changed);
}

template<class CostWriter>
void encode_core_event(
    binary_record_writer& out,
    const event::tabu_tenure_changed& value,
    CostWriter&)
{
    out.u64(value.evaluations);
    out.u64(value.iterations);
    out.u64(value.previous_tenure);
    out.u64(value.tenure);
}

constexpr std::uint8_t core_event_tag(const event::run_context&) noexcept
{
    return static_cast<std::uint8_t>(core_binary_event_tag::run_context);
}

template<class CostWriter>
void encode_core_event(
    binary_record_writer& out,
    const event::run_context& value,
    CostWriter&)
{
    out.string(value.stage);
    out.u64(value.stage_index);
    out.u64(value.attempt);
}

template<class Cost, class CostWriter>
    requires binary_cost_writer_for<CostWriter, Cost>
void encode_core_event(
    binary_record_writer& out,
    const event::run_started<Cost>& value,
    CostWriter& cost_writer)
{
    cost_writer(out, value.cost);
}

template<class Cost, class CostWriter>
    requires binary_cost_writer_for<CostWriter, Cost>
void encode_core_event(
    binary_record_writer& out,
    const event::move_evaluated<Cost>& value,
    CostWriter& cost_writer)
{
    out.u64(value.evaluations);
    out.u64(value.iterations);
    cost_writer(out, value.current_cost);
    cost_writer(out, value.candidate_cost);
    out.route(value.neighborhood);
}

template<class Cost, class CostWriter>
    requires binary_cost_writer_for<CostWriter, Cost>
void encode_core_event(
    binary_record_writer& out,
    const event::move_accepted<Cost>& value,
    CostWriter& cost_writer)
{
    out.u64(value.evaluations);
    out.u64(value.iterations);
    cost_writer(out, value.previous_cost);
    cost_writer(out, value.cost);
    out.route(value.neighborhood);
}

template<class Cost, class CostWriter>
    requires binary_cost_writer_for<CostWriter, Cost>
void encode_core_event(
    binary_record_writer& out,
    const event::incumbent_updated<Cost>& value,
    CostWriter& cost_writer)
{
    out.u64(value.evaluations);
    out.u64(value.iterations);
    cost_writer(out, value.previous_cost);
    cost_writer(out, value.cost);
}

template<class Cost, class CostWriter>
    requires binary_cost_writer_for<CostWriter, Cost>
void encode_core_event(
    binary_record_writer& out,
    const event::local_optimum<Cost>& value,
    CostWriter& cost_writer)
{
    out.u64(value.evaluations);
    out.u64(value.iterations);
    cost_writer(out, value.cost);
}

template<class CostWriter>
void encode_core_event(
    binary_record_writer& out,
    const event::neighborhood_selection& value,
    CostWriter&)
{
    out.u64(value.attempt);
    out.u64(value.child);
    out.f64(value.bias);
    out.f64(value.active_bias_total);
    out.f64(value.conditional_probability);
    out.boolean(value.produced_move);
    out.route(value.neighborhood);
}

template<class Cost, class CostWriter>
    requires binary_cost_writer_for<CostWriter, Cost>
void encode_core_event(
    binary_record_writer& out,
    const event::run_finished<Cost>& value,
    CostWriter& cost_writer)
{
    out.u64(value.evaluations);
    out.u64(value.iterations);
    cost_writer(out, value.cost);
    out.string(to_string(value.termination));
}

template<class Event, class CostWriter>
concept core_binary_event_for = requires(
    binary_record_writer& out,
    const Event& value,
    CostWriter& cost_writer) {
    { core_event_tag(value) } -> std::convertible_to<std::uint8_t>;
    encode_core_event(out, value, cost_writer);
};

template<class Event>
concept custom_binary_event = requires(
    binary_record_writer& out,
    const Event& value) {
    { binary_event_tag(value) } -> std::convertible_to<std::uint8_t>;
    encode_binary_event(out, value);
};

// A core event of the recorder's cost type (or without a cost), or an
// application event: a core event of another cost type, such as those of an
// until_feasible() stage on the hard cost, is not converted to Cost.
template<class Event, class Cost, class CostWriter>
inline constexpr bool binary_event_encodable_v =
    (core_event_of<Event, Cost> && core_binary_event_for<Event, CostWriter>)
    || (!core_event<Event> && custom_binary_event<Event>);

template<class Event, class CostWriter>
std::uint8_t event_tag(const Event& value, CostWriter&)
{
    if constexpr (core_binary_event_for<Event, CostWriter>)
    {
        return static_cast<std::uint8_t>(core_event_tag(value));
    }
    else
    {
        // Tags 1 to 127 are EasyLocal's: user_binary_event_tag gives the others.
        const auto tag = static_cast<std::uint8_t>(binary_event_tag(value));
        assert(tag >= 128 && "an application event's tag is user_binary_event_tag<N>()");
        return tag;
    }
}

template<class Event, class CostWriter>
void encode_event(
    binary_record_writer& out,
    const Event& value,
    CostWriter& cost_writer)
{
    if constexpr (core_binary_event_for<Event, CostWriter>)
    {
        encode_core_event(out, value, cost_writer);
    }
    else
    {
        encode_binary_event(out, value);
    }
}

template<class Cost, class CostWriter>
class binary_event_encoder
{
    static constexpr std::uint8_t first_user_tag = 128;

public:
    binary_event_encoder()
        requires std::default_initializable<CostWriter>
    = default;

    explicit binary_event_encoder(CostWriter cost_writer)
        noexcept(std::is_nothrow_move_constructible_v<CostWriter>)
        : cost_writer_{std::move(cost_writer)}
    {
    }

    template<class Event>
    static constexpr bool observes = binary_event_encodable_v<Event, Cost, CostWriter>;

    // Appends the header of the trace; with timestamps, the core events end
    // with the time since now.
    void append_header(std::vector<char>& buffer, const binary_buffer_options& options)
    {
        const auto fields = cost_writer_.fields();
        const std::vector<binary_field> cost_fields(
            std::ranges::begin(fields),
            std::ranges::end(fields));
        append_trace_header(buffer, cost_fields, options.metadata, options.timestamps);
        if (options.timestamps)
            start_ = std::chrono::steady_clock::now();
    }

    // Appends the record of value, after the schema of its tag the first
    // time; if the encoding throws, buffer and the schemas written are left
    // as they were.
    template<class Event>
        requires(binary_event_encodable_v<Event, Cost, CostWriter>)
    void append(std::vector<char>& buffer, const Event& value)
    {
        const auto tag = event_tag(value, cost_writer_);
        std::optional<std::chrono::steady_clock::duration> elapsed;
        if constexpr (core_binary_event_for<Event, CostWriter>)
        {
            if (start_)
                elapsed = std::chrono::steady_clock::now() - *start_;
        }
        const auto mark = buffer.size();
        bool* newly_described = nullptr;
        try
        {
            if constexpr (!core_binary_event_for<Event, CostWriter>
                && described_binary_event<Event>)
            {
                if (tag >= first_user_tag && !described_[tag - first_user_tag])
                {
                    const auto schema_offset = begin_record(
                        buffer,
                        static_cast<std::uint8_t>(core_binary_event_tag::schema));
                    binary_record_writer schema_out{buffer};
                    schema_out.schema(
                        tag,
                        describe_binary_event(std::type_identity<Event>{}));
                    finish_record(buffer, schema_offset);
                    newly_described = &described_[tag - first_user_tag];
                    *newly_described = true;
                }
            }
            const auto payload_offset = begin_record(buffer, tag);
            binary_record_writer out{buffer};
            encode_event(out, value, cost_writer_);
            if (elapsed)
            {
                out.u64(
                    static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(*elapsed)
                            .count()));
            }
            finish_record(buffer, payload_offset);
        }
        catch (...)
        {
            buffer.resize(mark);
            if (newly_described != nullptr)
                *newly_described = false;
            throw;
        }
    }

private:
    EASYLOCAL_NO_UNIQUE_ADDRESS CostWriter cost_writer_{};
    // The application tags whose schema is written.
    std::array<bool, 128> described_{};
    // When the recorder started, with timestamps.
    std::optional<std::chrono::steady_clock::time_point> start_;
};

class async_ostream_block_sink
{
public:
    async_ostream_block_sink(
        std::ostream& out,
        const std::size_t block_size,
        const std::size_t queue_blocks)
        : out_{out}
    {
        // The blocks are allocated here: an unlimited queue has no meaning.
        if (queue_blocks == (std::numeric_limits<std::size_t>::max)())
        {
            throw std::invalid_argument{
                "EasyLocal async trace recorder: async_queue_blocks must be finite"};
        }
        const auto count = std::max<std::size_t>(queue_blocks, 1U) + 1U;
        for (std::size_t index = 0; index < count; ++index)
        {
            std::vector<char> block;
            block.reserve(std::max<std::size_t>(block_size, 1U));
            free_.push_back(std::move(block));
        }
        worker_ = std::thread([this] { worker_loop(); });
    }

    async_ostream_block_sink(const async_ostream_block_sink&) = delete;
    async_ostream_block_sink& operator=(const async_ostream_block_sink&) = delete;

    ~async_ostream_block_sink()
    {
        stop();
    }

    // A free block; after an error, an empty block whose records are dropped.
    std::vector<char> acquire()
    {
        std::unique_lock lock{mutex_};
        free_cv_.wait(lock, [this] {
            return failed_.load(std::memory_order_relaxed) || !free_.empty();
        });
        if (failed_.load(std::memory_order_relaxed) && free_.empty())
        {
            return {};
        }
        auto block = std::move(free_.front());
        free_.pop_front();
        return block;
    }

    // Queues the block for the writer; after an error, drops it.
    void submit(std::vector<char>&& block)
    {
        if (block.empty())
        {
            return;
        }
        {
            std::lock_guard lock{mutex_};
            if (failed_.load(std::memory_order_relaxed))
            {
                block.clear();
                free_.push_back(std::move(block));
                return;
            }
            ready_.push_back(std::move(block));
        }
        ready_cv_.notify_one();
    }

    // Waits until the writer has written the queued blocks, or failed.
    void wait_idle()
    {
        std::unique_lock lock{mutex_};
        idle_cv_.wait(lock, [this] {
            return failed_.load(std::memory_order_relaxed) ||
                (ready_.empty() && !busy_);
        });
    }

    // Waits for the writer and flushes the stream: whether no error occurred.
    bool flush_output()
    {
        wait_idle();
        if (failed_.load(std::memory_order_relaxed))
            return false;
        try
        {
            out_.flush();
            if (!out_)
            {
                failed_.store(true, std::memory_order_relaxed);
            }
        }
        catch (...)
        {
            failed_.store(true, std::memory_order_relaxed);
        }
        return good();
    }

    [[nodiscard]]
    bool good() const noexcept
    {
        return !failed_.load(std::memory_order_relaxed);
    }

private:
    void worker_loop() noexcept
    {
        for (;;)
        {
            std::vector<char> block;
            {
                std::unique_lock lock{mutex_};
                ready_cv_.wait(lock, [this] {
                    return stopping_ || !ready_.empty();
                });
                if (stopping_ && ready_.empty())
                {
                    return;
                }
                block = std::move(ready_.front());
                ready_.pop_front();
                busy_ = true;
            }

            try
            {
                out_.write(block.data(), static_cast<std::streamsize>(block.size()));
                if (!out_)
                {
                    failed_.store(true, std::memory_order_relaxed);
                }
            }
            catch (...)
            {
                failed_.store(true, std::memory_order_relaxed);
            }

            block.clear();
            {
                std::lock_guard lock{mutex_};
                free_.push_back(std::move(block));
                busy_ = false;
                if (failed_.load(std::memory_order_relaxed))
                {
                    stopping_ = true;
                    while (!ready_.empty())
                    {
                        auto pending = std::move(ready_.front());
                        ready_.pop_front();
                        pending.clear();
                        free_.push_back(std::move(pending));
                    }
                }
            }
            free_cv_.notify_all();
            idle_cv_.notify_all();
        }
    }

    void stop() noexcept
    {
        {
            std::lock_guard lock{mutex_};
            stopping_ = true;
        }
        ready_cv_.notify_all();
        free_cv_.notify_all();
        if (worker_.joinable())
        {
            worker_.join();
        }
    }

    std::ostream& out_;
    mutable std::mutex mutex_;
    std::condition_variable ready_cv_;
    std::condition_variable free_cv_;
    std::condition_variable idle_cv_;
    std::deque<std::vector<char>> ready_;
    std::deque<std::vector<char>> free_;
    std::thread worker_;
    std::atomic_bool failed_{false};
    bool stopping_{false};
    bool busy_{false};
};

} // namespace detail

/// A tracer that writes the events as an ELTR binary stream, a block of
/// encoded records at a time.
///
/// The header is written and flushed at construction, and a block is written
/// to the stream when it reaches the block size, by flush() and by the
/// destructor. A run that ends without them, such as one that crashes, leaves
/// a trace that decodes, without the events of the last block: at most
/// `block_size` bytes. The stream is held by reference.
/// Requires a CostWriter callable as `writer(out, cost)`, with `fields()`.
template<class Cost, class CostWriter = default_binary_cost_writer<Cost>>
class buffered_binary_recorder
{
    static_assert(
        binary_cost_writer_for<CostWriter, Cost>,
        "an ELTR cost writer is callable as writer(binary_record_writer&, cost) and "
        "describes its fields with fields()");

    using encoder_type = detail::binary_event_encoder<Cost, CostWriter>;

public:
    /// The cost type of the events recorded.
    using cost_type = Cost;

    /// The version of the ELTR format written.
    static constexpr std::uint32_t format_version = detail::eltr_format_version;

    /// Writes to out, with a default-constructed cost writer.
    explicit buffered_binary_recorder(
        std::ostream& out,
        binary_buffer_options options = {})
        requires std::default_initializable<CostWriter>
        : out_{out},
          block_size_{std::max<std::size_t>(options.block_size, 1U)}
    {
        buffer_.reserve(block_size_);
        encoder_.append_header(buffer_, options);
        flush();
    }

    /// Writes to out, costs with cost_writer.
    buffered_binary_recorder(
        std::ostream& out,
        CostWriter cost_writer,
        binary_buffer_options options = {})
        : out_{out},
          encoder_{std::move(cost_writer)},
          block_size_{std::max<std::size_t>(options.block_size, 1U)}
    {
        buffer_.reserve(block_size_);
        encoder_.append_header(buffer_, options);
        flush();
    }

    /// Not copyable: it owns the output.
    buffered_binary_recorder(const buffered_binary_recorder&) = delete;
    /// Not copyable: it owns the output.
    buffered_binary_recorder& operator=(const buffered_binary_recorder&) = delete;

    /// Writes the pending records, ignoring an output error.
    ~buffered_binary_recorder()
    {
        try
        {
            write_pending();
        }
        catch (...)
        {
        }
    }

    /// Whether the recorder receives Event: a core event without a cost or of
    /// its cost type, or an application event with the ADL functions
    /// `binary_event_tag` and `encode_binary_event`.
    template<class Event>
    static constexpr bool observes = encoder_type::template observes<Event>;

    /// Encodes value as a record, writing the block when it is full.
    ///
    /// An application event with a schema is preceded, the first time, by the
    /// schema record of its tag.
    template<class Event>
        requires (encoder_type::template observes<Event>)
    void emit(const Event& value)
    {
        encoder_.append(buffer_, value);
        if (buffer_.size() >= block_size_)
        {
            write_pending();
        }
    }

    /// Writes the pending records and flushes the stream.
    void flush()
    {
        write_pending();
        out_.flush();
    }

    /// Whether the stream has had no error.
    [[nodiscard]]
    bool good() const
    {
        return out_.good();
    }

private:
    void write_pending()
    {
        if (buffer_.empty())
        {
            return;
        }
        out_.write(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
        buffer_.clear();
    }

    std::ostream& out_;
    encoder_type encoder_{};
    std::vector<char> buffer_;
    std::size_t block_size_{};
};

/// The binary recorder: the synchronous buffered_binary_recorder.
template<class Cost, class CostWriter = default_binary_cost_writer<Cost>>
using binary_recorder = buffered_binary_recorder<Cost, CostWriter>;

/// A tracer that writes the events as an ELTR binary stream from a background
/// writer thread, a block at a time.
///
/// The search fills a block and hands it to the thread, waiting when the queue
/// is full: no event is lost and the order is kept. One search only emits to
/// it. The stream is held by reference. The header is written and flushed at
/// construction, and the destructor writes the pending records: a run that
/// ends without it, such as one that crashes, loses the events of the blocks
/// not yet written, about `block_size` times `async_queue_blocks + 1` bytes.
///
/// An output error stops the recording, not the search: emit() drops the
/// events from then on, good() turns false and flush() throws.
/// Requires a CostWriter callable as `writer(out, cost)`, with `fields()`.
template<class Cost, class CostWriter = default_binary_cost_writer<Cost>>
class async_binary_recorder
{
    static_assert(
        binary_cost_writer_for<CostWriter, Cost>,
        "an ELTR cost writer is callable as writer(binary_record_writer&, cost) and "
        "describes its fields with fields()");

    using encoder_type = detail::binary_event_encoder<Cost, CostWriter>;

public:
    /// The cost type of the events recorded.
    using cost_type = Cost;

    /// The version of the ELTR format written.
    static constexpr std::uint32_t format_version = detail::eltr_format_version;

    /// Writes to out, with a default-constructed cost writer.
    ///
    /// Throws `std::invalid_argument` for an unlimited `async_queue_blocks`.
    explicit async_binary_recorder(
        std::ostream& out,
        binary_buffer_options options = {})
        requires std::default_initializable<CostWriter>
        : sink_{out, normalized_block_size(options), options.async_queue_blocks},
          block_size_{normalized_block_size(options)}
    {
        current_ = sink_.acquire();
        current_.reserve(block_size_);
        encoder_.append_header(current_, options);
        submit_current();
        static_cast<void>(sink_.flush_output());
    }

    /// Writes to out, costs with cost_writer.
    ///
    /// Throws `std::invalid_argument` for an unlimited `async_queue_blocks`.
    async_binary_recorder(
        std::ostream& out,
        CostWriter cost_writer,
        binary_buffer_options options = {})
        : sink_{out, normalized_block_size(options), options.async_queue_blocks},
          encoder_{std::move(cost_writer)},
          block_size_{normalized_block_size(options)}
    {
        current_ = sink_.acquire();
        current_.reserve(block_size_);
        encoder_.append_header(current_, options);
        submit_current();
        static_cast<void>(sink_.flush_output());
    }

    /// Not copyable: it owns the output and its writer thread.
    async_binary_recorder(const async_binary_recorder&) = delete;
    /// Not copyable: it owns the output and its writer thread.
    async_binary_recorder& operator=(const async_binary_recorder&) = delete;

    /// Writes the pending records and waits for the writer thread, ignoring an
    /// output error.
    ~async_binary_recorder()
    {
        try
        {
            drain();
        }
        catch (...)
        {
        }
    }

    /// Whether the recorder receives Event: a core event without a cost or of
    /// its cost type, or an application event with the ADL functions
    /// `binary_event_tag` and `encode_binary_event`.
    template<class Event>
    static constexpr bool observes = encoder_type::template observes<Event>;

    /// Encodes value as a record, handing the block to the writer thread when
    /// it is full; after an output error, drops it.
    ///
    /// An application event with a schema is preceded, the first time, by the
    /// schema record of its tag.
    template<class Event>
        requires (encoder_type::template observes<Event>)
    void emit(const Event& value)
    {
        if (!sink_.good())
            return;
        encoder_.append(current_, value);
        if (current_.size() >= block_size_)
        {
            submit_current();
        }
    }

    /// Hands the pending records to the writer thread, waits until it has
    /// written them and flushes the stream.
    ///
    /// Throws `std::ios_base::failure` if a write or this flush has failed.
    void flush()
    {
        submit_current();
        if (!sink_.flush_output())
            throw std::ios_base::failure{"EasyLocal async trace writer failed"};
    }

    /// Whether no write or flush of the stream has failed.
    [[nodiscard]]
    bool good() const noexcept
    {
        return sink_.good();
    }

private:
    static std::size_t normalized_block_size(const binary_buffer_options& options)
    {
        return std::max<std::size_t>(options.block_size, 1U);
    }

    void submit_current()
    {
        if (current_.empty())
        {
            return;
        }
        sink_.submit(std::move(current_));
        current_ = sink_.acquire();
        current_.reserve(block_size_);
    }

    void drain()
    {
        submit_current();
        sink_.wait_idle();
    }

    detail::async_ostream_block_sink sink_;
    encoder_type encoder_{};
    std::vector<char> current_;
    std::size_t block_size_{};
};

} // namespace easylocal::trace
