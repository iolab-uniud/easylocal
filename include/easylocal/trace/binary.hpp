#pragma once

#include <easylocal/trace/events.hpp>
#include <easylocal/trace/tracer.hpp>
#include <easylocal/utils/detail/attributes.hpp>

#include <algorithm>
#include <atomic>
#include <bit>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
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

// ELTR binary recording: encoders, buffered and asynchronous recorders.
namespace easylocal::trace
{

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
consteval std::uint8_t user_binary_event_tag()
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
    EASYLOCAL_NO_UNIQUE_ADDRESS CostWriter cost_writer_{};
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
