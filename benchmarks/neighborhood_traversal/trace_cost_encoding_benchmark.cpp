#include <easylocal/core/aggregation.hpp>
#include <easylocal/trace.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <streambuf>
#include <string_view>

namespace
{

class discard_streambuf : public std::streambuf
{
public:
    [[nodiscard]]
    auto bytes() const noexcept -> std::uint64_t
    {
        return bytes_;
    }

protected:
    auto xsputn(const char*, std::streamsize count) -> std::streamsize override
    {
        bytes_ += static_cast<std::uint64_t>(count);
        return count;
    }

    auto overflow(int_type character) -> int_type override
    {
        if (!traits_type::eq_int_type(character, traits_type::eof()))
        {
            ++bytes_;
        }
        return traits_type::not_eof(character);
    }

private:
    std::uint64_t bytes_{};
};

using scalar_cost = std::int64_t;
using lexicographic_cost =
    easylocal::aggregation::lexicographic_cost<std::int64_t, std::int64_t>;
using hierarchical_cost =
    easylocal::aggregation::hierarchical_cost<lexicographic_cost, std::int64_t>;

struct lexicographic_binary_cost_writer
{
    void operator()(
        easylocal::trace::binary_record_writer& out,
        const lexicographic_cost& cost) const
    {
        out.i64(cost.get<0>());
        out.i64(cost.get<1>());
    }
};

struct hierarchical_binary_cost_writer
{
    void operator()(
        easylocal::trace::binary_record_writer& out,
        const hierarchical_cost& cost) const
    {
        lexicographic_binary_cost_writer{}(out, cost.hard());
        out.i64(cost.soft());
    }
};

template<class Cost, class Writer>
void run_case(
    const std::string_view name,
    const Cost& current_cost,
    const Cost& candidate_cost,
    Writer writer)
{
    constexpr std::size_t warmup_events = 50'000;
    constexpr std::size_t measured_events = 1'000'000;

    discard_streambuf discarded;
    std::ostream output{&discarded};
    easylocal::trace::buffered_binary_recorder<Cost, Writer> recorder{
        output,
        writer};

    const easylocal::trace::event::move_evaluated<Cost> event{
        .evaluations = 42,
        .iterations = 7,
        .current_cost = current_cost,
        .candidate_cost = candidate_cost,
        .neighborhood = nullptr,
    };

    for (std::size_t index = 0; index < warmup_events; ++index)
    {
        recorder.emit(event);
    }
    recorder.flush();
    const auto warmup_bytes = discarded.bytes();

    const auto start = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < measured_events; ++index)
    {
        recorder.emit(event);
    }
    recorder.flush();
    const auto stop = std::chrono::steady_clock::now();

    const auto elapsed_ns =
        std::chrono::duration<double, std::nano>(stop - start).count();
    const auto measured_bytes = discarded.bytes() - warmup_bytes;

    std::cout << name << ','
              << elapsed_ns / static_cast<double>(measured_events) << ','
              << static_cast<double>(measured_bytes) /
                     static_cast<double>(measured_events)
              << ',' << measured_events << '\n';
}

} // namespace

int main()
{
    const auto lexicographic_current =
        easylocal::aggregation::lexicographic{}(std::int64_t{12}, std::int64_t{3});
    const auto lexicographic_candidate =
        easylocal::aggregation::lexicographic{}(std::int64_t{10}, std::int64_t{2});
    const auto hierarchical_current = easylocal::aggregation::hierarchical{}(
        lexicographic_current,
        std::int64_t{84});
    const auto hierarchical_candidate = easylocal::aggregation::hierarchical{}(
        lexicographic_candidate,
        std::int64_t{79});

    std::cout << "cost_model,ns_per_event,bytes_per_event,event_count\n";
    run_case<scalar_cost>(
        "scalar-i64",
        12,
        10,
        easylocal::trace::arithmetic_binary_cost_writer{});
    run_case(
        "lexicographic-2xi64",
        lexicographic_current,
        lexicographic_candidate,
        lexicographic_binary_cost_writer{});
    run_case(
        "hierarchical-2xi64-plus-i64",
        hierarchical_current,
        hierarchical_candidate,
        hierarchical_binary_cost_writer{});
}
