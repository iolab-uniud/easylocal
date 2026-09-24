#include "assignment_variants.hpp"
#include "tsp_variants.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string_view>
#include <vector>

namespace spike = easylocal::spike::neighborhood_authoring;
namespace assignment = easylocal::mwe::assignment;
namespace tsp = easylocal::mwe::tsp;

namespace allocation_probe
{

inline bool tracking = false;
inline std::size_t allocation_count = 0;
inline std::size_t allocated_bytes = 0;

void reset() noexcept
{
    allocation_count = 0;
    allocated_bytes = 0;
}

void record(const std::size_t bytes) noexcept
{
    if (tracking)
    {
        ++allocation_count;
        allocated_bytes += bytes;
    }
}

} // namespace allocation_probe

void* operator new(const std::size_t size)
{
    allocation_probe::record(size);
    if (auto* memory = std::malloc(size == 0 ? 1 : size))
    {
        return memory;
    }
    throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

void* operator new[](const std::size_t size)
{
    allocation_probe::record(size);
    if (auto* memory = std::malloc(size == 0 ? 1 : size))
    {
        return memory;
    }
    throw std::bad_alloc{};
}

void operator delete[](void* memory) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept
{
    std::free(memory);
}

namespace
{

struct AllocationResult
{
    std::size_t allocations{};
    std::size_t bytes{};
    std::size_t moves{};
    std::uint64_t checksum{};
};

template<class Explorer, class Solution, class Encode>
[[nodiscard]]
auto measure_allocations(
    const Explorer& explorer,
    const Solution& solution,
    Encode encode) -> AllocationResult
{
    allocation_probe::reset();
    allocation_probe::tracking = true;

    AllocationResult result;
    {
        auto moves = explorer.moves(solution);
        for (const auto& move : moves)
        {
            result.checksum += static_cast<std::uint64_t>(encode(move));
            ++result.moves;
        }
    }

    allocation_probe::tracking = false;
    result.allocations = allocation_probe::allocation_count;
    result.bytes = allocation_probe::allocated_bytes;
    return result;
}

template<class Explorer, class Solution, class Encode>
void report(
    const std::string_view domain,
    const std::string_view variant,
    const Explorer& explorer,
    const Solution& solution,
    Encode encode)
{
    const auto result = measure_allocations(explorer, solution, encode);
    std::cout
        << domain << ','
        << variant << ','
        << result.allocations << ','
        << result.bytes << ','
        << result.moves << ','
        << result.checksum << '\n';
}

void report_assignment()
{
    constexpr std::size_t jobs = 256;
    constexpr std::size_t machines = 32;

    assignment::Instance instance{
        .demand = std::vector<assignment::quantity_type>(jobs, 1),
        .capacity = std::vector<assignment::quantity_type>(machines, 10'000),
    };
    assignment::Solution solution{
        .assignment = std::vector<assignment::machine_id>(jobs),
    };
    for (std::size_t job = 0; job < jobs; ++job)
    {
        solution.assignment[job] = job % machines;
    }

    const assignment::SolutionManager manager{instance};
    const spike::assignment::CoroutineNeighborhoodExplorer coroutine_custom{manager};
#if EASYLOCAL_SPIKE_HAS_STD_GENERATOR
    const spike::assignment::StdCoroutineNeighborhoodExplorer coroutine_std{manager};
#endif
    const spike::assignment::CursorNeighborhoodExplorer cursor{manager};

    const auto encode = [](const assignment::Move& move) {
        return static_cast<std::uint64_t>(move.job) * 1'000'003ULL +
               static_cast<std::uint64_t>(move.destination);
    };

    report("assignment", "coroutine-custom", coroutine_custom, solution, encode);
#if EASYLOCAL_SPIKE_HAS_STD_GENERATOR
    report("assignment", "coroutine-std", coroutine_std, solution, encode);
#endif
    report("assignment", "cursor", cursor, solution, encode);
}

void report_tsp()
{
    constexpr std::size_t city_count = 256;

    tsp::Instance instance{
        .city_count = city_count,
        .distances = std::vector<tsp::distance_type>(
            city_count * city_count,
            0.0),
    };
    tsp::Solution solution{
        .tour = std::vector<tsp::city_id>(city_count),
    };
    for (std::size_t city = 0; city < city_count; ++city)
    {
        solution.tour[city] = city;
    }

    const tsp::SolutionManager manager{instance};
    const spike::tsp::CoroutineNeighborhoodExplorer coroutine_custom{manager};
#if EASYLOCAL_SPIKE_HAS_STD_GENERATOR
    const spike::tsp::StdCoroutineNeighborhoodExplorer coroutine_std{manager};
#endif
    const spike::tsp::CursorNeighborhoodExplorer cursor{manager};

    const auto encode = [](const tsp::TwoOptMove& move) {
        return static_cast<std::uint64_t>(move.first_edge) * 1'000'003ULL +
               static_cast<std::uint64_t>(move.second_edge);
    };

    report("tsp", "coroutine-custom", coroutine_custom, solution, encode);
#if EASYLOCAL_SPIKE_HAS_STD_GENERATOR
    report("tsp", "coroutine-std", coroutine_std, solution, encode);
#endif
    report("tsp", "cursor", cursor, solution, encode);
}

} // namespace

int main()
{
    std::cerr << "std::generator: "
              << (spike::has_std_generator ? "available" : "unavailable")
              << '\n';
    std::cout << "domain,variant,allocations,allocated_bytes,moves,checksum\n";
    report_assignment();
    report_tsp();
}
