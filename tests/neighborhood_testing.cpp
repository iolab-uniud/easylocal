#include <easylocal/testing/neighborhood.hpp>

#include "instance.hpp"
#include "neighborhood_explorer.hpp"
#include "solution.hpp"

#include <cstdint>
#include <optional>
#include <ranges>
#include <sstream>

namespace
{
using namespace easylocal::mwe::assignment;

struct AssignmentNeighborhoodContract
{
    using neighborhood = ReassignJobNeighborhoodExplorer;

    static auto instance() -> AssignmentInstance
    {
        return {
            .demand = {4, 3, 2},
            .capacity = {5, 5},
        };
    }

    static auto solution(const AssignmentInstance&) -> AssignmentSolution
    {
        return {.assignment = {0, 0, 1}};
    }

    static constexpr std::size_t random_samples = 16;
};

struct ProxyMove
{
    int delta{};
};

struct Move
{
    int delta{};
    Move() = default;
    explicit Move(const ProxyMove proxy) : delta{proxy.delta} {}
};

struct Solution
{
    int value{};
};

struct ProxyNeighborhood
{
    using move_type = Move;

    static auto is_valid(const Solution&, const Move& move) noexcept -> bool
    {
        return move.delta > 0;
    }

    static void make_move(Solution& solution, const Move& move) noexcept
    {
        solution.value += move.delta;
    }

    static auto moves(const Solution&)
    {
        return std::views::single(ProxyMove{1});
    }

    template<class RNG>
    static auto random_move(const Solution&, RNG&) -> std::optional<ProxyMove>
    {
        return ProxyMove{2};
    }
};

} // namespace

int main()
{
    static_assert(easylocal::native_moves_neighborhood_for<ProxyNeighborhood, Solution>);
    static_assert(easylocal::random_neighborhood_for<
        ProxyNeighborhood,
        Solution,
        easylocal::testing::deterministic_rng>);

    ProxyNeighborhood proxy;
    Solution solution;
    for (auto&& raw : easylocal::moves(proxy, solution))
    {
        Move move{raw};
        if (move.delta != 1)
        {
            return 1;
        }
    }

    easylocal::testing::deterministic_rng rng{7, 11};
    if (rng() != 7 || rng() != 11 || rng() != 7)
    {
        return 2;
    }
    rng.reset();

    const auto random = easylocal::random_move(proxy, solution, rng);
    if (!random || random->delta != 2)
    {
        return 3;
    }

    const auto report = easylocal::testing::neighborhood_contract<
        AssignmentNeighborhoodContract>();
    if (!report.passed() || report.checks() == 0)
    {
        return 4;
    }

    std::ostringstream output;
    easylocal::testing::print_report(output, report);
    if (output.str().find("checks passed") == std::string::npos)
    {
        return 5;
    }

    std::ostringstream run_output;
    if (easylocal::testing::run_contract<AssignmentNeighborhoodContract>(run_output) != 0)
    {
        return 6;
    }

    return 0;
}
