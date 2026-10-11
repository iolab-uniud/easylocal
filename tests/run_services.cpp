// A cost component, a delta cost component and a NeighborhoodExplorer built
// from the SolutionManager, which holds what the run needs: here a count kept
// incrementally, shared by the three and propagated from scratch only when the
// solution it follows is not the one it is asked about.
#include "support/expect.hpp"

#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/recipes.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/testing.hpp>
#include <easylocal/utils/generator.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace
{

struct Instance
{
    std::size_t size{};
};

// A solution of `size` bits, with the epoch the cache stamped it with: a tag,
// not part of the solution.
struct Solution
{
    std::vector<int> bit;
    mutable std::uint64_t epoch{0};

    bool operator==(const Solution& other) const
    {
        return bit == other.bit;
    }
};

struct FlipMove
{
    std::size_t position{};

    bool operator==(const FlipMove&) const = default;
};

// What a run needs: the number of bits set, kept incrementally, with the
// counters the test reads.
class SetBitCache
{
public:
    std::uint64_t sync(const Solution& solution) const
    {
        if (solution.epoch == epoch_ && solution.epoch != 0)
            return solution.epoch;

        ++propagations;
        set_bits_ = 0;
        for (const auto bit : solution.bit)
            set_bits_ += bit;
        return ++epoch_;
    }

    std::int64_t set_bits() const noexcept
    {
        return set_bits_;
    }

    // What the count would become, without keeping it.
    std::int64_t probe(const Solution& solution, const FlipMove& move) const
    {
        return set_bits_ + (solution.bit[move.position] != 0 ? -1 : 1);
    }

    std::uint64_t commit(const Solution& solution, const FlipMove& move) const
    {
        set_bits_ = probe(solution, move);
        return ++epoch_;
    }

    // How many times the count was propagated from scratch.
    mutable std::size_t propagations{0};

private:
    mutable std::int64_t set_bits_{0};
    mutable std::uint64_t epoch_{0};
};

// The SolutionManager declares the cache as the services of a run: one is
// built for each bind, and handed to the component, to its delta and to the
// explorer.
class Manager : public easylocal::solution_manager_base<Instance, Solution>
{
public:
    using services_type = SetBitCache;

    using solution_manager_base::solution_manager_base;

    bool is_valid(const Solution& solution) const
    {
        return solution.bit.size() == input().size;
    }

    Solution initial_solution() const
    {
        return Solution{.bit = std::vector<int>(input().size, 1)};
    }

    template<std::uniform_random_bit_generator RNG>
    Solution random_solution(RNG& rng) const
    {
        std::bernoulli_distribution draw{0.5};
        Solution solution{.bit = std::vector<int>(input().size, 0)};
        for (auto& bit : solution.bit)
            bit = draw(rng) ? 1 : 0;
        return solution;
    }
};

// Built from the Input and the services of the run: it reads the cache.
class SetBits
{
public:
    static std::string_view name()
    {
        return "SetBits";
    }

    SetBits(const Instance&, SetBitCache& cache) noexcept : cache_{&cache} {}

    std::int64_t evaluate(const Solution& solution) const
    {
        solution.epoch = cache_->sync(solution);
        return cache_->set_bits();
    }

private:
    SetBitCache* cache_;
};

class SetBitsDelta
{
public:
    SetBitsDelta(const Instance&, SetBitCache& cache) noexcept : cache_{&cache} {}

    std::int64_t delta_evaluate(const Solution& solution, const FlipMove& move) const
    {
        solution.epoch = cache_->sync(solution);
        return cache_->probe(solution, move) - cache_->set_bits();
    }

private:
    SetBitCache* cache_;
};

class FlipExplorer : public easylocal::neighborhood_explorer_base<Manager, FlipMove>
{
public:
    // From the SolutionManager and the services of the run.
    FlipExplorer(const Manager& manager, SetBitCache& cache) noexcept
        : neighborhood_explorer_base{manager}, cache_{&cache}
    {
    }

    static constexpr std::string_view name()
    {
        return "Flip";
    }

    easylocal::generator<FlipMove> moves(const Solution& solution) const
    {
        for (std::size_t position = 0; position < solution.bit.size(); ++position)
            co_yield FlipMove{position};
    }

    bool is_valid(const Solution& solution, const FlipMove& move) const
    {
        return move.position < solution.bit.size();
    }

    template<std::uniform_random_bit_generator RNG>
    std::optional<FlipMove> random_move(const Solution& solution, RNG& rng) const
    {
        if (solution.bit.empty())
            return std::nullopt;
        std::uniform_int_distribution<std::size_t> draw{0, solution.bit.size() - 1};
        return FlipMove{draw(rng)};
    }

    // The cache follows the committed move, as a declared model's network does.
    void make_move(Solution& solution, const FlipMove& move) const
    {
        solution.epoch = cache_->sync(solution);
        const auto epoch = cache_->commit(solution, move);
        solution.bit[move.position] = solution.bit[move.position] != 0 ? 0 : 1;
        solution.epoch = epoch;
    }

private:
    SetBitCache* cache_;
};

} // namespace

int main()
{
    bool ok = true;

    const Instance instance{.size = 12};

    auto runner = easylocal::make_runner<easylocal::runners::FirstImprovement>()
        | (easylocal::solution_manager<Manager>() | easylocal::component<SetBits>())
        | (easylocal::neighborhood<FlipExplorer>()
            | easylocal::delta<SetBits, SetBitsDelta>());

    auto bound = runner.bind(instance);

    const auto& cache = bound.solution_manager().services();
    const auto result = bound.run(Solution{.bit = std::vector<int>(instance.size, 1)});

    ok &= expect(result.cost == 0, "the search reaches the solution with no bit set");
    ok &= expect(
        cache.propagations <= 2,
        "the component, its delta and the explorer share one cache, propagated "
        "once for the initial solution");

    // The contract checks build the component and its delta the same way.
    namespace elt = easylocal::testing;
    const elt::fixture<Manager> fixture{
        instance,
        Solution{.bit = {1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0}},
    };

    const auto checks = elt::run_checks(
        elt::check_solution_manager(fixture),
        elt::check_cost_component<SetBits>(fixture),
        elt::check_neighborhood<FlipExplorer>(fixture),
        elt::check_delta_cost_component<FlipExplorer, SetBits, SetBitsDelta>(fixture));

    ok &= expect(checks == 0, "the contract checks pass on the components of a run");

    return ok ? 0 : 1;
}
