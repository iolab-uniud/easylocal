#include "assignment_variants.hpp"
#include "tsp_variants.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace bench = easylocal::benchmark::neighborhood_traversal;
namespace assignment = easylocal::mwe::assignment;
namespace tsp = easylocal::mwe::tsp;

namespace
{


template<std::ranges::input_range Range, class Encode>
[[nodiscard]]
auto encoded_moves(Range&& moves, Encode encode) -> std::vector<std::uint64_t>
{
    std::vector<std::uint64_t> encoded;
    for (const auto& move : moves)
    {
        encoded.push_back(encode(move));
    }
    return encoded;
}

[[nodiscard]]
auto validate_assignment_variants() -> bool
{
    const assignment::Instance instance{
        .demand = {2, 3, 5, 7},
        .capacity = {10, 10, 10},
    };
    const assignment::Solution solution{
        .assignment = {0, 1, 2, 0},
    };
    const assignment::SolutionManager manager{instance};

    const bench::assignment::CoroutineNeighborhoodExplorer coroutine{manager};
#if EASYLOCAL_BENCHMARK_HAS_STD_GENERATOR
    const bench::assignment::StdCoroutineNeighborhoodExplorer std_coroutine{manager};
#endif
    const assignment::NeighborhoodExplorer cursor{manager};

    const auto encode = [](const assignment::Move& move) {
        return static_cast<std::uint64_t>(move.job) * 1024ULL +
               static_cast<std::uint64_t>(move.destination);
    };

    const auto expected = encoded_moves(cursor.moves(solution), encode);
    if (expected != encoded_moves(coroutine.moves(solution), encode))
    {
        return false;
    }
#if EASYLOCAL_BENCHMARK_HAS_STD_GENERATOR
    if (expected != encoded_moves(std_coroutine.moves(solution), encode))
    {
        return false;
    }
#endif
    return true;
}

[[nodiscard]]
auto validate_tsp_variants() -> bool
{
    constexpr std::size_t city_count = 6;
    const tsp::Instance instance{
        .city_count = city_count,
        .distances = std::vector<tsp::distance_type>(
            city_count * city_count,
            0.0),
    };
    const tsp::Solution solution{
        .tour = {0, 1, 2, 3, 4, 5},
    };
    const tsp::SolutionManager manager{instance};

    const bench::tsp::CoroutineNeighborhoodExplorer coroutine{manager};
#if EASYLOCAL_BENCHMARK_HAS_STD_GENERATOR
    const bench::tsp::StdCoroutineNeighborhoodExplorer std_coroutine{manager};
#endif
    const tsp::NeighborhoodExplorer cursor{manager};

    const auto encode = [](const tsp::TwoOptMove& move) {
        return static_cast<std::uint64_t>(move.first_edge) * 1024ULL +
               static_cast<std::uint64_t>(move.second_edge);
    };

    const auto expected = encoded_moves(cursor.moves(solution), encode);
    if (expected != encoded_moves(coroutine.moves(solution), encode))
    {
        return false;
    }
#if EASYLOCAL_BENCHMARK_HAS_STD_GENERATOR
    if (expected != encoded_moves(std_coroutine.moves(solution), encode))
    {
        return false;
    }
#endif
    return true;
}

struct ScanResult
{
    std::uint64_t checksum{};
    std::size_t move_count{};
};

inline void observe(const std::uint64_t value) noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    // Keep every produced move observable to the optimizer without adding a
    // volatile memory access to the measured hot loop. GCC and Clang may still
    // optimize the traversal machinery itself, which is exactly what this benchmark
    // wants to measure, but they cannot delete the per-move production entirely.
    __asm__ __volatile__("" : : "r"(value));
#else
    volatile auto sink = value;
    (void)sink;
#endif
}

template<class Explorer, class Solution, class Consumer>
[[nodiscard]]
auto scan_once(
    const Explorer& explorer,
    const Solution& solution,
    Consumer consume) -> ScanResult
{
    ScanResult result;

    for (const auto& move : explorer.moves(solution))
    {
        const auto token = static_cast<std::uint64_t>(consume(move));
        observe(token);
        result.checksum += token;
        ++result.move_count;
    }

    observe(result.checksum);
    return result;
}

template<class Explorer, class Solution, class Consumer>
void run_case(
    const std::string_view domain,
    const std::string_view workload,
    const std::string_view variant,
    const Explorer& explorer,
    const Solution& solution,
    Consumer consume,
    const std::size_t target_moves,
    const std::size_t trials)
{
    const auto reference = scan_once(explorer, solution, consume);

    if (reference.move_count == 0)
    {
        std::cerr << domain << '/' << variant
                  << ": empty benchmark neighborhood\n";
        return;
    }

    const auto repetitions = std::max<std::size_t>(
        1,
        target_moves / reference.move_count);

    // Warm up code and data paths without including them in measurements.
    for (std::size_t warmup = 0; warmup < 3; ++warmup)
    {
        observe(scan_once(explorer, solution, consume).checksum);
    }

    for (std::size_t trial = 0; trial < trials; ++trial)
    {
        std::uint64_t checksum = 0;
        const auto start = std::chrono::steady_clock::now();

        for (std::size_t repetition = 0;
             repetition < repetitions;
             ++repetition)
        {
            checksum += scan_once(explorer, solution, consume).checksum;
        }

        const auto stop = std::chrono::steady_clock::now();
        const auto elapsed =
            std::chrono::duration<double, std::nano>(stop - start).count();
        const auto measured_moves = repetitions * reference.move_count;
        const auto ns_per_move =
            elapsed / static_cast<double>(measured_moves);

        observe(checksum);

        std::cout
            << domain << ','
            << workload << ','
            << variant << ','
            << reference.move_count << ','
            << trial << ','
            << repetitions << ','
            << measured_moves << ','
            << ns_per_move << ','
            << checksum << '\n';
    }
}

[[nodiscard]]
auto parse_positive(
    const int argc,
    char** argv,
    const int index,
    const std::size_t fallback,
    const std::string_view name) -> std::size_t
{
    if (argc <= index)
    {
        return fallback;
    }

    std::size_t result = 0;
    const std::string_view text{argv[index]};
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), result);

    if (error != std::errc{} || end != text.data() + text.size() || result == 0)
    {
        std::cerr
            << "invalid " << name << " '" << text
            << "'; using " << fallback << '\n';
        return fallback;
    }

    return result;
}

[[nodiscard]]
auto parse_seed(
    const int argc,
    char** argv,
    const int index,
    const std::uint64_t fallback) -> std::uint64_t
{
    if (argc <= index)
    {
        return fallback;
    }

    std::uint64_t result = 0;
    const std::string_view text{argv[index]};
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), result);

    if (error != std::errc{} || end != text.data() + text.size())
    {
        std::cerr
            << "invalid seed '" << text
            << "'; using " << fallback << '\n';
        return fallback;
    }

    return result;
}

void benchmark_assignment(
    const std::string_view scale,
    const std::size_t jobs,
    const std::size_t machines,
    const std::size_t target_moves,
    const std::size_t trials,
    const std::uint64_t seed)
{
    assignment::Instance instance{
        .demand = std::vector<assignment::quantity_type>(jobs),
        .capacity = std::vector<assignment::quantity_type>(machines),
    };
    assignment::Solution solution{
        .assignment = std::vector<assignment::machine_id>(jobs),
    };

    for (std::size_t job = 0; job < jobs; ++job)
    {
        instance.demand[job] = static_cast<assignment::quantity_type>(
            1 + ((job * 17ULL + seed) % 31ULL));
        solution.assignment[job] = job % machines;
    }

    for (std::size_t machine = 0; machine < machines; ++machine)
    {
        instance.capacity[machine] = static_cast<assignment::quantity_type>(
            10'000 + ((machine * 131ULL + seed) % 4096ULL));
    }

    const assignment::SolutionManager manager{instance};
    const bench::assignment::CoroutineNeighborhoodExplorer coroutine_custom{manager};
#if EASYLOCAL_BENCHMARK_HAS_STD_GENERATOR
    const bench::assignment::StdCoroutineNeighborhoodExplorer coroutine_std{manager};
#endif
    const assignment::NeighborhoodExplorer cursor{manager};

    const auto traversal_consumer = [](const assignment::Move& move) {
        return static_cast<std::uint64_t>(move.job) * 1'000'003ULL +
               static_cast<std::uint64_t>(move.destination);
    };

    const auto light_consumer = [&](const assignment::Move& move) {
        const auto source = solution.assignment[move.job];
        auto token = traversal_consumer(move);
        token ^= (static_cast<std::uint64_t>(source) + 1ULL) * 65'537ULL;
        token ^= static_cast<std::uint64_t>(instance.demand[move.job]) *
                 1'000'000'007ULL;
        token ^= static_cast<std::uint64_t>(instance.capacity[move.destination]);
        return token;
    };

    const auto run_workload = [&](const std::string_view workload, auto consume) {
        const auto domain = std::string{"assignment-"} + std::string{scale};
        run_case(domain, workload, "coroutine-custom", coroutine_custom,
                 solution, consume, target_moves, trials);
#if EASYLOCAL_BENCHMARK_HAS_STD_GENERATOR
        run_case(domain, workload, "coroutine-std", coroutine_std, solution,
                 consume, target_moves, trials);
#endif
        run_case(domain, workload, "cursor", cursor, solution, consume,
                 target_moves, trials);
    };

    run_workload("traversal", traversal_consumer);
    run_workload("light-consumer", light_consumer);
}

void benchmark_tsp(
    const std::string_view scale,
    const std::size_t city_count,
    const std::size_t target_moves,
    const std::size_t trials,
    const std::uint64_t seed)
{
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

    for (std::size_t first = 0; first < city_count; ++first)
    {
        for (std::size_t second = first + 1; second < city_count; ++second)
        {
            const auto raw =
                ((first + 1ULL) * 131'071ULL) ^
                ((second + 1ULL) * 524'287ULL) ^ seed;
            const auto distance =
                static_cast<tsp::distance_type>(1 + (raw % 65'535ULL)) / 16.0;
            instance.distances[first * city_count + second] = distance;
            instance.distances[second * city_count + first] = distance;
        }
    }

    const tsp::SolutionManager manager{instance};
    const bench::tsp::CoroutineNeighborhoodExplorer coroutine_custom{manager};
#if EASYLOCAL_BENCHMARK_HAS_STD_GENERATOR
    const bench::tsp::StdCoroutineNeighborhoodExplorer coroutine_std{manager};
#endif
    const tsp::NeighborhoodExplorer cursor{manager};

    const auto traversal_consumer = [](const tsp::TwoOptMove& move) {
        return static_cast<std::uint64_t>(move.first_edge) * 1'000'003ULL +
               static_cast<std::uint64_t>(move.second_edge);
    };

    const auto light_consumer = [&](const tsp::TwoOptMove& move) {
        const auto first = solution.tour[move.first_edge];
        const auto first_next =
            solution.tour[(move.first_edge + 1) % solution.tour.size()];
        const auto second = solution.tour[move.second_edge];
        const auto second_next =
            solution.tour[(move.second_edge + 1) % solution.tour.size()];

        auto token = traversal_consumer(move);
        token ^= std::bit_cast<std::uint64_t>(
            instance.distance(first, first_next));
        token ^= std::bit_cast<std::uint64_t>(
            instance.distance(second, second_next));
        token ^= std::bit_cast<std::uint64_t>(
            instance.distance(first, second));
        token ^= std::bit_cast<std::uint64_t>(
            instance.distance(first_next, second_next));
        return token;
    };

    const auto run_workload = [&](const std::string_view workload, auto consume) {
        const auto domain = std::string{"tsp-"} + std::string{scale};
        run_case(domain, workload, "coroutine-custom", coroutine_custom,
                 solution, consume, target_moves, trials);
#if EASYLOCAL_BENCHMARK_HAS_STD_GENERATOR
        run_case(domain, workload, "coroutine-std", coroutine_std, solution,
                 consume, target_moves, trials);
#endif
        run_case(domain, workload, "cursor", cursor, solution, consume,
                 target_moves, trials);
    };

    run_workload("traversal", traversal_consumer);
    run_workload("light-consumer", light_consumer);
}

} // namespace

int main(const int argc, char** argv)
{
    constexpr std::size_t default_target_moves = 2'000'000;
    constexpr std::size_t default_trials = 3;
    constexpr std::uint64_t default_seed = 123'456'789ULL;

    const auto target_moves = parse_positive(
        argc, argv, 1, default_target_moves, "target move count");
    const auto trials = parse_positive(
        argc, argv, 2, default_trials, "trial count");
    const auto seed = parse_seed(argc, argv, 3, default_seed);

    std::cerr << "std::generator: "
              << (bench::has_std_generator ? "available" : "unavailable")
              << '\n';

    std::cout
        << "domain,workload,variant,neighborhood_size,trial,repetitions,"
           "measured_moves,ns_per_move,checksum\n";

    if (!validate_assignment_variants() || !validate_tsp_variants())
    {
        std::cerr << "neighborhood traversal variants disagree\n";
        return 1;
    }

    benchmark_assignment("medium", 256, 32, target_moves, trials, seed);
    benchmark_tsp("medium", 256, target_moves, trials, seed);

    return 0;
}
