#include "capacity_delta.hpp"
#include "cost_components.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/aggregation.hpp>
#include <easylocal/runner.hpp>
#include <easylocal/search/first_improvement.hpp>
#include <easylocal/trace.hpp>

#if defined(EASYLOCAL_TRACE_BENCHMARK_HAS_SPDLOG)
#include <spdlog/details/log_msg.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#endif

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <streambuf>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace assignment = easylocal::mwe::assignment;

namespace
{

struct counting_tracer
{
    template<class Event>
    static constexpr bool observes = true;

    template<class Event>
    void emit(const Event&) noexcept
    {
        ++events;
    }

    std::uint64_t events{};
};

class discard_streambuf : public std::streambuf
{
protected:
    auto xsputn(const char*, std::streamsize count) -> std::streamsize override
    {
        return count;
    }

    auto overflow(int_type character) -> int_type override
    {
        return traits_type::not_eof(character);
    }
};

#if defined(EASYLOCAL_TRACE_BENCHMARK_HAS_SPDLOG)

class formatting_discard_sink final
    : public spdlog::sinks::base_sink<spdlog::details::null_mutex>
{
public:
    [[nodiscard]]
    auto bytes() const noexcept -> std::uint64_t
    {
        return bytes_;
    }

protected:
    void sink_it_(const spdlog::details::log_msg& message) override
    {
        spdlog::memory_buf_t formatted;
        formatter_->format(message, formatted);
        bytes_ += formatted.size();
    }

    void flush_() override
    {
    }

private:
    std::uint64_t bytes_{};
};

inline void append_route_json(
    std::string& result,
    const easylocal::trace::neighborhood_route_node* node)
{
    if (node == nullptr)
    {
        return;
    }
    append_route_json(result, node->parent);
    if (result.size() > 1)
    {
        result += ',';
    }
    result += std::to_string(node->child);
}

inline auto route_json(const easylocal::trace::neighborhood_route_node* node)
    -> std::string
{
    std::string result{"["};
    append_route_json(result, node);
    result += ']';
    return result;
}

template<class Function>
void with_route_json(
    const easylocal::trace::neighborhood_route_node* node,
    Function&& function)
{
    if (node == nullptr)
    {
        function(std::string_view{"[]"});
        return;
    }
    const auto route = route_json(node);
    function(std::string_view{route});
}

template<class Cost>
class spdlog_jsonl_tracer
{
public:
    explicit spdlog_jsonl_tracer(spdlog::logger& logger) noexcept
        : logger_{logger}
    {
    }

    template<class Event>
    static constexpr bool observes = true;

    void emit(const easylocal::trace::event::run_started<Cost>& value)
    {
        logger_.info(R"({{"event":"run_started","cost":{}}})", value.cost);
    }

    void emit(const easylocal::trace::event::move_evaluated<Cost>& value)
    {
        with_route_json(value.neighborhood, [&](const std::string_view route) {
            logger_.info(
                R"({{"event":"move_evaluated","evaluations":{},"iterations":{},"current_cost":{},"candidate_cost":{},"neighborhood":{}}})",
                value.evaluations,
                value.iterations,
                value.current_cost,
                value.candidate_cost,
                route);
        });
    }

    void emit(const easylocal::trace::event::move_accepted<Cost>& value)
    {
        with_route_json(value.neighborhood, [&](const std::string_view route) {
            logger_.info(
                R"({{"event":"move_accepted","evaluations":{},"iterations":{},"previous_cost":{},"cost":{},"neighborhood":{}}})",
                value.evaluations,
                value.iterations,
                value.previous_cost,
                value.cost,
                route);
        });
    }

    void emit(const easylocal::trace::event::incumbent_updated<Cost>& value)
    {
        logger_.info(
            R"({{"event":"incumbent_updated","evaluations":{},"iterations":{},"previous_cost":{},"cost":{}}})",
            value.evaluations,
            value.iterations,
            value.previous_cost,
            value.cost);
    }

    void emit(const easylocal::trace::event::local_optimum<Cost>& value)
    {
        logger_.info(
            R"({{"event":"local_optimum","evaluations":{},"iterations":{},"cost":{}}})",
            value.evaluations,
            value.iterations,
            value.cost);
    }

    void emit(const easylocal::trace::event::neighborhood_selection& value)
    {
        with_route_json(value.neighborhood, [&](const std::string_view route) {
            logger_.info(
                R"({{"event":"neighborhood_selection","attempt":{},"child":{},"bias":{},"active_bias_total":{},"conditional_probability":{},"produced_move":{},"neighborhood":{}}})",
                value.attempt,
                value.child,
                value.bias,
                value.active_bias_total,
                value.conditional_probability,
                value.produced_move,
                route);
        });
    }

    void emit(const easylocal::trace::event::run_finished<Cost>& value)
    {
        logger_.info(
            R"({{"event":"run_finished","evaluations":{},"iterations":{},"cost":{}}})",
            value.evaluations,
            value.iterations,
            value.cost);
    }

private:
    spdlog::logger& logger_;
};

#endif

template<class Function>
auto measure(Function&& function, std::size_t repetitions)
{
    constexpr std::size_t warmup_repetitions = 8;
    for (std::size_t repetition = 0; repetition < warmup_repetitions; ++repetition)
    {
        (void)function();
    }

    const auto start = std::chrono::steady_clock::now();
    std::uint64_t checksum = 0;
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
    {
        checksum += static_cast<std::uint64_t>(function());
    }
    const auto stop = std::chrono::steady_clock::now();
    const auto ns = std::chrono::duration<double, std::nano>(stop - start).count();
    return std::pair{ns, checksum};
}

} // namespace

int main()
{
    constexpr std::size_t jobs = 96;
    constexpr std::size_t machines = 8;
    constexpr std::size_t repetitions = 64;

    assignment::AssignmentInstance instance{
        .demand = std::vector<assignment::quantity_type>(jobs),
        .capacity = std::vector<assignment::quantity_type>(machines, 64),
    };
    assignment::AssignmentSolution initial{
        .assignment = std::vector<assignment::machine_id>(jobs, 0),
    };
    for (std::size_t job = 0; job < jobs; ++job)
    {
        instance.demand[job] = static_cast<assignment::quantity_type>(1 + (job * 17) % 9);
    }

    auto runner = easylocal::Runner{
        easylocal::search::FirstImprovement{{.max_evaluations = 500'000}}}
        | (easylocal::solution_manager<assignment::AssignmentSolutionManager>()
           | easylocal::component<assignment::CapacityCostComponent>()
           | easylocal::aggregator([](const assignment::CapacityValue& capacity) {
                 return capacity.total_overload;
             }))
        | (easylocal::neighborhood<assignment::ReassignJobNeighborhoodExplorer>()
           | easylocal::delta<
                 assignment::CapacityCostComponent,
                 assignment::ReassignCapacityDeltaEvaluator>());
    auto bound = runner.bind(instance);

    const auto reference = bound.run(initial);
    using bound_type = std::remove_reference_t<decltype(bound)>;
    using cost_type = typename bound_type::cost_type;

    const auto baseline = measure([&] {
        const auto result = bound.run(initial);
        return result.evaluations;
    }, repetitions);

    easylocal::trace::null_tracer null;
    const auto explicit_null = measure([&] {
        const auto result = bound.run(initial, null);
        return result.evaluations;
    }, repetitions);

    counting_tracer counter;
    const auto counting = measure([&] {
        const auto result = bound.run(initial, counter);
        return result.evaluations;
    }, repetitions);

    const auto memory = measure([&] {
        easylocal::trace::memory_recorder<cost_type> recorder;
        const auto result = bound.run(initial, recorder);
        return result.evaluations;
    }, repetitions);

    discard_streambuf discarded;
    std::ostream discarded_output{&discarded};
    easylocal::trace::jsonl_recorder<cost_type> jsonl{discarded_output};
    const auto streaming = measure([&] {
        const auto result = bound.run(initial, jsonl);
        return result.evaluations;
    }, repetitions);

#if defined(EASYLOCAL_TRACE_BENCHMARK_HAS_SPDLOG)
    auto spdlog_sink = std::make_shared<formatting_discard_sink>();
    spdlog::logger spdlog_logger{"easylocal-trace-benchmark", spdlog_sink};
    spdlog_logger.set_pattern("%v");
    spdlog_jsonl_tracer<cost_type> spdlog_trace{spdlog_logger};
    const auto spdlog_streaming = measure([&] {
        const auto result = bound.run(initial, spdlog_trace);
        return result.evaluations;
    }, repetitions);
#endif

    const auto evaluations = static_cast<double>(reference.evaluations * repetitions);
    std::cout << "mode,ns_per_evaluation,checksum\n";
    std::cout << "baseline," << baseline.first / evaluations << ',' << baseline.second << '\n';
    std::cout << "explicit-null," << explicit_null.first / evaluations << ',' << explicit_null.second << '\n';
    std::cout << "counting," << counting.first / evaluations << ',' << counting.second << '\n';
    std::cout << "memory," << memory.first / evaluations << ',' << memory.second << '\n';
    std::cout << "jsonl-discard," << streaming.first / evaluations << ',' << streaming.second << '\n';
#if defined(EASYLOCAL_TRACE_BENCHMARK_HAS_SPDLOG)
    std::cout << "spdlog-jsonl-discard," << spdlog_streaming.first / evaluations << ','
              << spdlog_streaming.second << '\n';
    std::cerr << "spdlog_formatted_bytes=" << spdlog_sink->bytes() << '\n';
#else
    std::cerr << "spdlog=unavailable\n";
#endif
    std::cerr << "counted_events=" << counter.events << '\n';

    const auto checksum = baseline.second;
    const bool checksums_match =
        explicit_null.second == checksum && counting.second == checksum &&
        memory.second == checksum && streaming.second == checksum
#if defined(EASYLOCAL_TRACE_BENCHMARK_HAS_SPDLOG)
        && spdlog_streaming.second == checksum
#endif
        ;
    return checksums_match ? 0 : 2;
}
