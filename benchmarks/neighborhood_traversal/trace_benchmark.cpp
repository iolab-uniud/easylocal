#include "capacity_delta.hpp"
#include "cost_components.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/core/aggregation.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/trace.hpp>



#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
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


[[nodiscard]]
auto result_token(const auto& result) noexcept -> std::uint64_t
{
    auto value = static_cast<std::uint64_t>(result.cost);
    value ^= static_cast<std::uint64_t>(result.evaluations) * 0x9e3779b97f4a7c15ULL;
    value ^= static_cast<std::uint64_t>(result.termination) << 57U;
    for (std::size_t index = 0; index < result.solution.assignment.size(); ++index)
    {
        value ^= (static_cast<std::uint64_t>(result.solution.assignment[index]) + 1U)
            * (0x100000001b3ULL + static_cast<std::uint64_t>(index));
    }
    return value;
}

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

struct async_measurement
{
    double producer_ns{};
    double total_ns{};
    std::uint64_t checksum{};
};

template<class Function, class Flush>
auto measure_async(
    Function&& function,
    Flush&& flush,
    std::size_t repetitions) -> async_measurement
{
    constexpr std::size_t warmup_repetitions = 8;
    for (std::size_t repetition = 0; repetition < warmup_repetitions; ++repetition)
    {
        (void)function();
    }
    flush();

    const auto start = std::chrono::steady_clock::now();
    std::uint64_t checksum = 0;
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
    {
        checksum += static_cast<std::uint64_t>(function());
    }
    const auto producer_stop = std::chrono::steady_clock::now();
    flush();
    const auto total_stop = std::chrono::steady_clock::now();

    return {
        .producer_ns = std::chrono::duration<double, std::nano>(
            producer_stop - start).count(),
        .total_ns = std::chrono::duration<double, std::nano>(
            total_stop - start).count(),
        .checksum = checksum,
    };
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
        easylocal::runners::FirstImprovement{{.max_evaluations = 500'000}}}
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
        return result_token(result);
    }, repetitions);

    easylocal::trace::null_tracer null;
    const auto explicit_null = measure([&] {
        const auto result = bound.run(initial, easylocal::with(null));
        return result_token(result);
    }, repetitions);

    counting_tracer counter;
    const auto counting = measure([&] {
        const auto result = bound.run(initial, easylocal::with(counter));
        return result_token(result);
    }, repetitions);

    const auto memory = measure([&] {
        easylocal::trace::memory_recorder<cost_type> recorder;
        const auto result = bound.run(initial, easylocal::with(recorder));
        return result_token(result);
    }, repetitions);

    discard_streambuf binary_discarded;
    std::ostream binary_discarded_output{&binary_discarded};
    easylocal::trace::buffered_binary_recorder<cost_type> binary{
        binary_discarded_output};
    const auto binary_streaming = measure([&] {
        const auto result = bound.run(initial, easylocal::with(binary));
        return result_token(result);
    }, repetitions);
    binary.flush();

    discard_streambuf async_binary_discarded;
    std::ostream async_binary_discarded_output{&async_binary_discarded};
    easylocal::trace::async_binary_recorder<cost_type> async_binary{
        async_binary_discarded_output};
    const auto async_binary_streaming = measure_async(
        [&] {
            const auto result = bound.run(initial, easylocal::with(async_binary));
            return result_token(result);
        },
        [&] { async_binary.flush(); },
        repetitions);

    const auto temp_directory = std::filesystem::temp_directory_path();
    const auto buffered_file_path =
        temp_directory / "easylocal-trace-benchmark-buffered.eltrace";
    const auto async_file_path =
        temp_directory / "easylocal-trace-benchmark-async.eltrace";

    std::error_code remove_error;
    std::filesystem::remove(buffered_file_path, remove_error);
    std::filesystem::remove(async_file_path, remove_error);

    std::pair<double, std::uint64_t> binary_file;
    std::uintmax_t binary_file_bytes{};
    {
        std::ofstream output{buffered_file_path, std::ios::binary | std::ios::trunc};
        {
            easylocal::trace::buffered_binary_recorder<cost_type> recorder{output};
            binary_file = measure([&] {
                const auto result = bound.run(initial, easylocal::with(recorder));
                return result_token(result);
            }, repetitions);
            recorder.flush();
        }
        output.close();
        binary_file_bytes = std::filesystem::file_size(buffered_file_path);
    }

    async_measurement async_binary_file;
    std::uintmax_t async_binary_file_bytes{};
    {
        std::ofstream output{async_file_path, std::ios::binary | std::ios::trunc};
        {
            easylocal::trace::async_binary_recorder<cost_type> recorder{output};
            async_binary_file = measure_async(
                [&] {
                    const auto result = bound.run(initial, easylocal::with(recorder));
                    return result_token(result);
                },
                [&] { recorder.flush(); },
                repetitions);
        }
        output.close();
        async_binary_file_bytes = std::filesystem::file_size(async_file_path);
    }

    std::filesystem::remove(buffered_file_path, remove_error);
    std::filesystem::remove(async_file_path, remove_error);



    const auto evaluations = static_cast<double>(reference.evaluations * repetitions);
    std::cout << "mode,ns_per_evaluation,checksum\n";
    std::cout << "baseline," << baseline.first / evaluations << ',' << baseline.second << '\n';
    std::cout << "explicit-null," << explicit_null.first / evaluations << ',' << explicit_null.second << '\n';
    std::cout << "counting," << counting.first / evaluations << ',' << counting.second << '\n';
    std::cout << "memory," << memory.first / evaluations << ',' << memory.second << '\n';
    std::cout << "binary-buffered-discard," << binary_streaming.first / evaluations << ','
              << binary_streaming.second << '\n';
    std::cout << "binary-async-producer-discard,"
              << async_binary_streaming.producer_ns / evaluations << ','
              << async_binary_streaming.checksum << '\n';
    std::cout << "binary-async-total-discard,"
              << async_binary_streaming.total_ns / evaluations << ','
              << async_binary_streaming.checksum << '\n';
    std::cout << "binary-buffered-file," << binary_file.first / evaluations << ','
              << binary_file.second << '\n';
    std::cout << "binary-async-producer-file,"
              << async_binary_file.producer_ns / evaluations << ','
              << async_binary_file.checksum << '\n';
    std::cout << "binary-async-total-file,"
              << async_binary_file.total_ns / evaluations << ','
              << async_binary_file.checksum << '\n';

    std::cerr << "counted_events=" << counter.events << '\n';
    if (counter.events != 0)
    {
        const auto events = static_cast<double>(counter.events);
        std::cerr << "binary_bytes=" << binary_discarded.bytes()
                  << ",binary_bytes_per_event=" << binary_discarded.bytes() / events << '\n';
        std::cerr << "async_binary_bytes=" << async_binary_discarded.bytes()
                  << ",async_binary_bytes_per_event="
                  << async_binary_discarded.bytes() / events << '\n';
        std::cerr << "binary_file_bytes=" << binary_file_bytes
                  << ",async_binary_file_bytes=" << async_binary_file_bytes << '\n';
    }

    const auto checksum = baseline.second;
    const bool checksums_match =
        explicit_null.second == checksum && counting.second == checksum &&
        memory.second == checksum && binary_streaming.second == checksum &&
        async_binary_streaming.checksum == checksum &&
        binary_file.second == checksum &&
        async_binary_file.checksum == checksum

        ;
    return checksums_match ? 0 : 2;
}
