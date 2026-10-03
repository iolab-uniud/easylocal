#pragma once

#include <easylocal/trace/events.hpp>
#include <easylocal/trace/tracer.hpp>
#include <easylocal/utils/detail/attributes.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <exception>
#include <iterator>
#include <mutex>
#include <ostream>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// ELTR binary recording: encoders, buffered and asynchronous recorders. A
// trace describes itself: its header gives the metadata of the run, the layout
// of the costs and the fields of every event (docs/tracing.md).
namespace easylocal::trace
{

// The types of the fields of an ELTR record.
enum class binary_type : std::uint8_t
{
    u8 = 1,
    i8 = 2,
    u16 = 3,
    i16 = 4,
    u32 = 5,
    i32 = 6,
    u64 = 7,
    i64 = 8,
    f32 = 9,
    f64 = 10,
    boolean = 11,
    string = 12, // u32 size, then UTF-8 bytes
    bytes = 13,  // u32 size, then the bytes
    route = 14,  // u32 count, then a u32 per level
    cost = 15,   // the trace's cost layout
};

struct binary_field
{
    std::string name;
    binary_type type;
};

// The name and fields of the records of one tag.
struct binary_event_schema
{
    std::string name;
    std::vector<binary_field> fields;
};

class binary_record_writer
{
public:
    explicit binary_record_writer(std::vector<char>& buffer) noexcept
        : buffer_{buffer}
    {
    }

    void u8(const std::uint8_t value)
    {
        buffer_.push_back(static_cast<char>(value));
    }

    void i8(const std::int8_t value)
    {
        u8(static_cast<std::uint8_t>(value));
    }

    void boolean(const bool value)
    {
        u8(value ? 1U : 0U);
    }

    void u16(const std::uint16_t value)
    {
        append_unsigned_le(value);
    }

    void i16(const std::int16_t value)
    {
        append_unsigned_le(static_cast<std::uint16_t>(value));
    }

    void u32(const std::uint32_t value)
    {
        append_unsigned_le(value);
    }

    void i32(const std::int32_t value)
    {
        append_unsigned_le(static_cast<std::uint32_t>(value));
    }

    void u64(const std::uint64_t value)
    {
        append_unsigned_le(value);
    }

    void i64(const std::int64_t value)
    {
        u64(static_cast<std::uint64_t>(value));
    }

    void f32(const float value)
    {
        u32(std::bit_cast<std::uint32_t>(value));
    }

    void f64(const double value)
    {
        u64(std::bit_cast<std::uint64_t>(value));
    }

    // A field list: u32 count, then the name and the type of each field.
    void fields(const std::span<const binary_field> value)
    {
        u32(static_cast<std::uint32_t>(value.size()));
        for (const auto& field : value)
        {
            string(field.name);
            u8(static_cast<std::uint8_t>(field.type));
        }
    }

    void schema(const std::uint8_t tag, const binary_event_schema& value)
    {
        u8(tag);
        string(value.name);
        fields(value.fields);
    }

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

    void bytes(const std::span<const std::byte> value)
    {
        u32(static_cast<std::uint32_t>(value.size()));
        raw_bytes(value.data(), value.size());
    }

    void string(const std::string_view value)
    {
        u32(static_cast<std::uint32_t>(value.size()));
        raw_bytes(value.data(), value.size());
    }

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

// A cost writer writes a cost and describes what it writes: fields() gives
// the fields in order, a scalar cost one field with an empty name.
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

// The cost types of easylocal::cost, recognized by their shape (the trace
// layer does not depend on them): a cost::lexicographic has levels and
// get<Index>(), a cost::hierarchical hard() and soft().
template<class Cost, std::size_t Index>
using level_type =
    std::remove_cvref_t<decltype(std::declval<const Cost&>().template get<Index>())>;

template<class Cost, std::size_t... Index>
consteval bool levels_encodable(std::index_sequence<Index...>)
{
    return (default_binary_cost<level_type<Cost, Index>>::value && ...);
}

template<class Cost>
concept leveled_cost = requires {
    { Cost::levels } -> std::convertible_to<std::size_t>;
} && levels_encodable<Cost>(std::make_index_sequence<Cost::levels>{});

template<class Cost>
using hard_type = std::remove_cvref_t<decltype(std::declval<const Cost&>().hard())>;

template<class Cost>
using soft_type = std::remove_cvref_t<decltype(std::declval<const Cost&>().soft())>;

template<class Cost>
concept hard_soft_cost =
    requires(const Cost& cost) {
        cost.hard();
        cost.soft();
    } && default_binary_cost<hard_type<Cost>>::value
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

// The cost writer of the recorders by default: an arithmetic cost as i64, u64
// or f64; a cost::lexicographic as its levels ("0", "1", ...) and a
// cost::hierarchical as "hard" and "soft", nested as the types are.
template<class Cost>
struct default_binary_cost_writer
{
    static_assert(
        detail::default_binary_cost<Cost>::value,
        "no default ELTR encoding for this cost type: give the recorder a cost writer "
        "with operator()(binary_record_writer&, const Cost&) and fields()");

    void operator()(binary_record_writer& out, const Cost& cost) const
    {
        detail::default_binary_cost<Cost>::write(out, cost);
    }

    [[nodiscard]]
    std::vector<binary_field> fields() const
    {
        std::vector<binary_field> result;
        detail::default_binary_cost<Cost>::describe(result, {});
        return result;
    }
};

// An application event may describe its records with an ADL function
// describe_binary_event(std::type_identity<Event>) returning its
// binary_event_schema; the recorder then writes
// the schema before the first record of its tag, and decoders name its fields.
template<class Event>
concept described_binary_event = requires {
    {
        describe_binary_event(std::type_identity<Event>{})
    } -> std::convertible_to<binary_event_schema>;
};

template<std::uint8_t Index>
consteval std::uint8_t user_binary_event_tag()
{
    static_assert(Index < 128, "EasyLocal user binary event tags have indices 0..127");
    return static_cast<std::uint8_t>(128U + Index);
}

struct binary_buffer_options
{
    std::size_t block_size = 256U * 1024U;
    std::size_t async_queue_blocks = 4;
    // Key-value pairs written in the header: the instance, the runner, the
    // seed, the parameters, whatever tells the run apart.
    std::vector<std::pair<std::string, std::string>> metadata{};
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
};

inline void finish_record(std::vector<char>& buffer, std::size_t payload_offset);

inline void append_u32_le(std::vector<char>& buffer, const std::uint32_t value)
{
    binary_record_writer out{buffer};
    out.u32(value);
}

// The schemas of the core events, by tag; each matches its encode_core_event.
inline std::vector<std::pair<std::uint8_t, binary_event_schema>> core_event_schemas()
{
    using enum binary_type;
    const binary_field evaluations{"evaluations", u64};
    const binary_field iterations{"iterations", u64};
    const auto tag = [](const core_binary_event_tag value) {
        return static_cast<std::uint8_t>(value);
    };
    using enum core_binary_event_tag;
    return {
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
        {tag(run_finished), {"run_finished", {evaluations, iterations, {"cost", cost}}}},
        {tag(solution_visited),
            {"solution_visited",
                {evaluations, iterations, {"hash", u64}, {"cost", cost}}}},
        {tag(aspiration_applied),
            {"aspiration_applied", {evaluations, iterations, {"cost", cost}}}},
        {tag(tabu_escape), {"tabu_escape", {evaluations, iterations, {"moves", u64}}}},
        {tag(tabu_tenure_changed),
            {"tabu_tenure_changed",
                {evaluations, iterations, {"previous_tenure", u64}, {"tenure", u64}}}},
    };
}

// "ELTR", the format version, then the header: its size (u32), the metadata
// (u32 count, key and value strings), the cost layout (a field list) and the
// core event schemas (u32 count, then tag, name and field list each).
inline void append_trace_header(
    std::vector<char>& buffer,
    const std::span<const binary_field> cost_fields,
    const std::span<const std::pair<std::string, std::string>> metadata)
{
    static constexpr char magic[] = {'E', 'L', 'T', 'R'};
    buffer.insert(buffer.end(), std::begin(magic), std::end(magic));
    append_u32_le(buffer, 1U);

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
    const auto schemas = core_event_schemas();
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

template<class Event, class CostWriter>
inline constexpr bool binary_event_encodable_v =
    core_binary_event_for<Event, CostWriter> || custom_binary_event<Event>;

template<class Event, class CostWriter>
std::uint8_t event_tag(const Event& value, CostWriter&)
{
    if constexpr (core_binary_event_for<Event, CostWriter>)
    {
        return static_cast<std::uint8_t>(core_event_tag(value));
    }
    else
    {
        return static_cast<std::uint8_t>(binary_event_tag(value));
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

template<class CostWriter>
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
    static constexpr bool observes = binary_event_encodable_v<Event, CostWriter>;

    void append_header(
        std::vector<char>& buffer,
        const std::span<const std::pair<std::string, std::string>> metadata) const
    {
        const auto fields = cost_writer_.fields();
        const std::vector<binary_field> cost_fields(
            std::ranges::begin(fields),
            std::ranges::end(fields));
        append_trace_header(buffer, cost_fields, metadata);
    }

    template<class Event>
        requires (binary_event_encodable_v<Event, CostWriter>)
    void append(std::vector<char>& buffer, const Event& value)
    {
        const auto tag = event_tag(value, cost_writer_);
        if constexpr (!core_binary_event_for<Event, CostWriter>
            && described_binary_event<Event>)
        {
            if (tag >= first_user_tag && !described_[tag - first_user_tag])
            {
                described_[tag - first_user_tag] = true;
                const auto schema_offset = begin_record(
                    buffer,
                    static_cast<std::uint8_t>(core_binary_event_tag::schema));
                binary_record_writer schema_out{buffer};
                schema_out.schema(
                    tag,
                    describe_binary_event(std::type_identity<Event>{}));
                finish_record(buffer, schema_offset);
            }
        }
        const auto payload_offset = begin_record(buffer, tag);
        binary_record_writer out{buffer};
        encode_event(out, value, cost_writer_);
        finish_record(buffer, payload_offset);
    }

private:
    EASYLOCAL_NO_UNIQUE_ADDRESS CostWriter cost_writer_{};
    // The application tags whose schema is written.
    std::array<bool, 128> described_{};
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

    std::vector<char> acquire()
    {
        std::unique_lock lock{mutex_};
        free_cv_.wait(lock, [this] {
            return failed_.load(std::memory_order_relaxed) || !free_.empty();
        });
        if (failed_.load(std::memory_order_relaxed))
        {
            throw std::ios_base::failure{"EasyLocal async trace writer failed"};
        }
        auto block = std::move(free_.front());
        free_.pop_front();
        return block;
    }

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
                throw std::ios_base::failure{"EasyLocal async trace writer failed"};
            }
            ready_.push_back(std::move(block));
        }
        ready_cv_.notify_one();
    }

    void wait_idle()
    {
        std::unique_lock lock{mutex_};
        idle_cv_.wait(lock, [this] {
            return failed_.load(std::memory_order_relaxed) ||
                (ready_.empty() && !busy_);
        });
        if (failed_.load(std::memory_order_relaxed))
        {
            throw std::ios_base::failure{"EasyLocal async trace writer failed"};
        }
    }

    void flush_output()
    {
        wait_idle();
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

template<class Cost, class CostWriter = default_binary_cost_writer<Cost>>
class buffered_binary_recorder
{
    static_assert(
        binary_cost_writer_for<CostWriter, Cost>,
        "an ELTR cost writer is callable as writer(binary_record_writer&, cost) and "
        "describes its fields with fields()");

    using encoder_type = detail::binary_event_encoder<CostWriter>;

public:
    static constexpr std::uint16_t format_version = 1;

    explicit buffered_binary_recorder(
        std::ostream& out,
        binary_buffer_options options = {})
        requires std::default_initializable<CostWriter>
        : out_{out},
          block_size_{std::max<std::size_t>(options.block_size, 1U)}
    {
        buffer_.reserve(block_size_);
        encoder_.append_header(buffer_, options.metadata);
    }

    buffered_binary_recorder(
        std::ostream& out,
        CostWriter cost_writer,
        binary_buffer_options options = {})
        noexcept(std::is_nothrow_move_constructible_v<CostWriter>)
        : out_{out},
          encoder_{std::move(cost_writer)},
          block_size_{std::max<std::size_t>(options.block_size, 1U)}
    {
        buffer_.reserve(block_size_);
        encoder_.append_header(buffer_, options.metadata);
    }

    buffered_binary_recorder(const buffered_binary_recorder&) = delete;
    buffered_binary_recorder& operator=(const buffered_binary_recorder&) = delete;

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

    template<class Event>
    static constexpr bool observes = encoder_type::template observes<Event>;

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

    void flush()
    {
        write_pending();
        out_.flush();
    }

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

template<class Cost, class CostWriter = default_binary_cost_writer<Cost>>
using binary_recorder = buffered_binary_recorder<Cost, CostWriter>;

template<class Cost, class CostWriter = default_binary_cost_writer<Cost>>
class async_binary_recorder
{
    static_assert(
        binary_cost_writer_for<CostWriter, Cost>,
        "an ELTR cost writer is callable as writer(binary_record_writer&, cost) and "
        "describes its fields with fields()");

    using encoder_type = detail::binary_event_encoder<CostWriter>;

public:
    static constexpr std::uint16_t format_version = 1;

    explicit async_binary_recorder(
        std::ostream& out,
        binary_buffer_options options = {})
        requires std::default_initializable<CostWriter>
        : sink_{out, normalized_block_size(options), options.async_queue_blocks},
          block_size_{normalized_block_size(options)}
    {
        current_ = sink_.acquire();
        current_.reserve(block_size_);
        encoder_.append_header(current_, options.metadata);
    }

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
        encoder_.append_header(current_, options.metadata);
    }

    async_binary_recorder(const async_binary_recorder&) = delete;
    async_binary_recorder& operator=(const async_binary_recorder&) = delete;

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

    template<class Event>
    static constexpr bool observes = encoder_type::template observes<Event>;

    template<class Event>
        requires (encoder_type::template observes<Event>)
    void emit(const Event& value)
    {
        encoder_.append(current_, value);
        if (current_.size() >= block_size_)
        {
            submit_current();
        }
    }

    void flush()
    {
        submit_current();
        sink_.flush_output();
    }

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
