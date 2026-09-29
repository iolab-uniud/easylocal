#pragma once

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdint>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <deque>
#include <exception>
#include <mutex>
#include <ostream>
#include <span>
#include <string_view>
#include <thread>
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

    void boolean(const bool value)
    {
        u8(value ? 1U : 0U);
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

    void f64(const double value)
    {
        u64(std::bit_cast<std::uint64_t>(value));
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
    static auto route_size(const neighborhood_route_node* node) noexcept -> std::size_t
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

template<class Writer, class Cost>
concept binary_cost_writer_for = requires(
    Writer& writer,
    binary_record_writer& out,
    const Cost& cost) {
    writer(out, cost);
};

struct arithmetic_binary_cost_writer
{
    template<class Cost>
    void operator()(binary_record_writer& out, const Cost& cost) const
        requires std::is_arithmetic_v<Cost>
    {
        if constexpr (std::floating_point<Cost>)
        {
            out.f64(static_cast<double>(cost));
        }
        else if constexpr (std::signed_integral<Cost>)
        {
            out.i64(static_cast<std::int64_t>(cost));
        }
        else
        {
            out.u64(static_cast<std::uint64_t>(cost));
        }
    }
};

template<std::uint8_t Index>
consteval auto user_binary_event_tag() -> std::uint8_t
{
    static_assert(Index < 128, "EasyLocal user binary event tags have indices 0..127");
    return static_cast<std::uint8_t>(128U + Index);
}

struct binary_buffer_options
{
    std::size_t block_size = 256U * 1024U;
    std::size_t async_queue_blocks = 4;
};

namespace detail
{

enum class core_binary_event_tag : std::uint8_t
{
    run_started = 1,
    move_evaluated = 2,
    move_accepted = 3,
    incumbent_updated = 4,
    local_optimum = 5,
    neighborhood_selection = 6,
    run_finished = 7,
};

inline void append_u32_le(std::vector<char>& buffer, const std::uint32_t value)
{
    binary_record_writer out{buffer};
    out.u32(value);
}

inline void append_trace_header(std::vector<char>& buffer)
{
    static constexpr char magic[] = {'E', 'L', 'T', 'R'};
    buffer.insert(buffer.end(), std::begin(magic), std::end(magic));
    append_u32_le(buffer, 1U);
}

inline auto begin_record(
    std::vector<char>& buffer,
    const std::uint8_t tag) -> std::size_t
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
constexpr auto core_event_tag(const event::run_started<Cost>&) noexcept -> std::uint8_t
{
    return static_cast<std::uint8_t>(core_binary_event_tag::run_started);
}

template<class Cost>
constexpr auto core_event_tag(const event::move_evaluated<Cost>&) noexcept -> std::uint8_t
{
    return static_cast<std::uint8_t>(core_binary_event_tag::move_evaluated);
}

template<class Cost>
constexpr auto core_event_tag(const event::move_accepted<Cost>&) noexcept -> std::uint8_t
{
    return static_cast<std::uint8_t>(core_binary_event_tag::move_accepted);
}

template<class Cost>
constexpr auto core_event_tag(const event::incumbent_updated<Cost>&) noexcept -> std::uint8_t
{
    return static_cast<std::uint8_t>(core_binary_event_tag::incumbent_updated);
}

template<class Cost>
constexpr auto core_event_tag(const event::local_optimum<Cost>&) noexcept -> std::uint8_t
{
    return static_cast<std::uint8_t>(core_binary_event_tag::local_optimum);
}

constexpr auto core_event_tag(const event::neighborhood_selection&) noexcept -> std::uint8_t
{
    return static_cast<std::uint8_t>(core_binary_event_tag::neighborhood_selection);
}

template<class Cost>
constexpr auto core_event_tag(const event::run_finished<Cost>&) noexcept -> std::uint8_t
{
    return static_cast<std::uint8_t>(core_binary_event_tag::run_finished);
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
auto event_tag(const Event& value, CostWriter&) -> std::uint8_t
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

    template<class Event>
        requires (binary_event_encodable_v<Event, CostWriter>)
    void append(std::vector<char>& buffer, const Event& value)
    {
        const auto payload_offset = begin_record(buffer, event_tag(value, cost_writer_));
        binary_record_writer out{buffer};
        encode_event(out, value, cost_writer_);
        finish_record(buffer, payload_offset);
    }

private:
    [[no_unique_address]] CostWriter cost_writer_{};
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
    auto operator=(const async_ostream_block_sink&)
        -> async_ostream_block_sink& = delete;

    ~async_ostream_block_sink()
    {
        stop();
    }

    auto acquire() -> std::vector<char>
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
    auto good() const noexcept -> bool
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

template<class Cost, class CostWriter = arithmetic_binary_cost_writer>
class buffered_binary_recorder
{
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
        detail::append_trace_header(buffer_);
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
        detail::append_trace_header(buffer_);
    }

    buffered_binary_recorder(const buffered_binary_recorder&) = delete;
    auto operator=(const buffered_binary_recorder&)
        -> buffered_binary_recorder& = delete;

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
    auto good() const -> bool
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

template<class Cost, class CostWriter = arithmetic_binary_cost_writer>
using binary_recorder = buffered_binary_recorder<Cost, CostWriter>;

template<class Cost, class CostWriter = arithmetic_binary_cost_writer>
class async_binary_recorder
{
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
        detail::append_trace_header(current_);
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
        detail::append_trace_header(current_);
    }

    async_binary_recorder(const async_binary_recorder&) = delete;
    auto operator=(const async_binary_recorder&) -> async_binary_recorder& = delete;

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
    auto good() const noexcept -> bool
    {
        return sink_.good();
    }

private:
    static auto normalized_block_size(const binary_buffer_options& options)
        -> std::size_t
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
