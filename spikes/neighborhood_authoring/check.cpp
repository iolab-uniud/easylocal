#include "assignment_variants.hpp"
#include "tsp_variants.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <ranges>
#include <vector>

namespace spike = easylocal::spike::neighborhood_authoring;
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
auto check_assignment() -> bool
{
    const assignment::Instance instance{
        .demand = {2, 3, 5, 7},
        .capacity = {10, 10, 10},
    };
    const assignment::Solution solution{
        .assignment = {0, 1, 2, 0},
    };
    const assignment::SolutionManager manager{instance};

    const spike::assignment::CoroutineNeighborhoodExplorer coroutine{manager};
#if EASYLOCAL_SPIKE_HAS_STD_GENERATOR
    const spike::assignment::StdCoroutineNeighborhoodExplorer std_coroutine{manager};
#endif
    const spike::assignment::CursorNeighborhoodExplorer cursor{manager};

    static_assert(std::ranges::input_range<decltype(coroutine.moves(solution))>);
    static_assert(!std::ranges::forward_range<decltype(coroutine.moves(solution))>);
#if EASYLOCAL_SPIKE_HAS_STD_GENERATOR
    static_assert(std::ranges::input_range<decltype(std_coroutine.moves(solution))>);
#endif
    static_assert(std::ranges::input_range<decltype(cursor.moves(solution))>);
    static_assert(!std::ranges::forward_range<decltype(cursor.moves(solution))>);

    const auto encode = [](const assignment::Move& move) {
        return static_cast<std::uint64_t>(move.job) * 1024ULL +
               static_cast<std::uint64_t>(move.destination);
    };

    const std::vector<std::uint64_t> expected{
        1ULL, 2ULL,
        1024ULL, 1026ULL,
        2048ULL, 2049ULL,
        3073ULL, 3074ULL,
    };
    const auto coroutine_moves = encoded_moves(coroutine.moves(solution), encode);
#if EASYLOCAL_SPIKE_HAS_STD_GENERATOR
    const auto std_coroutine_moves =
        encoded_moves(std_coroutine.moves(solution), encode);
#endif
    const auto cursor_moves = encoded_moves(cursor.moves(solution), encode);

    return expected == coroutine_moves &&
#if EASYLOCAL_SPIKE_HAS_STD_GENERATOR
           expected == std_coroutine_moves &&
#endif
           expected == cursor_moves;
}

[[nodiscard]]
auto check_tsp() -> bool
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

    const spike::tsp::CoroutineNeighborhoodExplorer coroutine{manager};
#if EASYLOCAL_SPIKE_HAS_STD_GENERATOR
    const spike::tsp::StdCoroutineNeighborhoodExplorer std_coroutine{manager};
#endif
    const spike::tsp::CursorNeighborhoodExplorer cursor{manager};

    static_assert(std::ranges::input_range<decltype(coroutine.moves(solution))>);
    static_assert(!std::ranges::forward_range<decltype(coroutine.moves(solution))>);
#if EASYLOCAL_SPIKE_HAS_STD_GENERATOR
    static_assert(std::ranges::input_range<decltype(std_coroutine.moves(solution))>);
#endif
    static_assert(std::ranges::input_range<decltype(cursor.moves(solution))>);
    static_assert(!std::ranges::forward_range<decltype(cursor.moves(solution))>);

    const auto encode = [](const tsp::TwoOptMove& move) {
        return static_cast<std::uint64_t>(move.first_edge) * 1024ULL +
               static_cast<std::uint64_t>(move.second_edge);
    };

    const std::vector<std::uint64_t> expected{
        2ULL, 3ULL, 4ULL,
        1027ULL, 1028ULL, 1029ULL,
        2052ULL, 2053ULL,
        3077ULL,
    };
    const auto coroutine_moves = encoded_moves(coroutine.moves(solution), encode);
#if EASYLOCAL_SPIKE_HAS_STD_GENERATOR
    const auto std_coroutine_moves =
        encoded_moves(std_coroutine.moves(solution), encode);
#endif
    const auto cursor_moves = encoded_moves(cursor.moves(solution), encode);

    return expected == coroutine_moves &&
#if EASYLOCAL_SPIKE_HAS_STD_GENERATOR
           expected == std_coroutine_moves &&
#endif
           expected == cursor_moves;
}

} // namespace

int main()
{
    std::cerr << "std::generator: "
              << (spike::has_std_generator ? "available" : "unavailable")
              << '\n';
    const auto assignment_ok = check_assignment();
    const auto tsp_ok = check_tsp();

    if (!assignment_ok)
    {
        std::cerr << "assignment traversal mismatch\n";
    }

    if (!tsp_ok)
    {
        std::cerr << "TSP traversal mismatch\n";
    }

    if (!assignment_ok || !tsp_ok)
    {
        return 1;
    }

    std::cout << "all neighborhood-authoring variants preserve traversal order\n";
    return 0;
}
