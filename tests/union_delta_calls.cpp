// The deltas of a neighborhood union: a component is evaluated by the deltas of
// the children when every child has one for it, each child with its own, and
// in full for the moves of every child when one child has none.
#include "../examples/tutorial/tsp.hpp"

#include <easylocal/helpers/neighborhood_union.hpp>
#include <easylocal/runners/best_improvement.hpp>
#include <easylocal/runners/runner.hpp>

#include <cassert>
#include <cstddef>
#include <utility>

namespace
{

using namespace tutorial;
namespace el = easylocal;

// The 2-opt delta of the tutorial, counting its calls.
class CountedTwoOptDelta
{
public:
    static inline std::size_t calls = 0;

    explicit CountedTwoOptDelta(const Tsp& input) : delta_{input} {}

    [[nodiscard]]
    double delta_evaluate(const Tour& tour, const TwoOpt& move) const
    {
        ++calls;
        return delta_.delta_evaluate(tour, move);
    }

private:
    TwoOptLengthDelta delta_;
};

// The length change of a swap of two cities, counting its calls: the length
// of the swapped tour less that of the tour.
class CountedSwapDelta
{
public:
    static inline std::size_t calls = 0;

    explicit CountedSwapDelta(const Tsp& input) : input_{input} {}

    [[nodiscard]]
    double delta_evaluate(const Tour& tour, const SwapCities& move) const
    {
        ++calls;
        auto swapped = tour;
        std::swap(swapped.order[move.i], swapped.order[move.j]);
        return length(swapped) - length(tour);
    }

private:
    [[nodiscard]]
    double length(const Tour& tour) const
    {
        const auto n = tour.order.size();
        double total = 0.0;
        for (std::size_t k = 0; k < n; ++k)
            total += input_.distance[tour.order[k]][tour.order[(k + 1) % n]];
        return total;
    }

    const Tsp& input_;
};

template<class Neighborhood>
std::size_t run_best_improvement(Neighborhood neighborhood)
{
    const auto input = five_cities();
    auto runner = el::make_runner<el::runners::BestImprovement>({.max_evaluations = 200})
        | (el::solution_manager<TourManager>() | el::component<TourLength>())
        | std::move(neighborhood);
    auto search = runner.bind(input);
    const auto result = search.run(TourManager{input}.initial_solution());
    return result.evaluations;
}

} // namespace

int main()
{
    // Both children have a delta for the tour length: each move is evaluated
    // by its child's delta.
    CountedTwoOptDelta::calls = 0;
    CountedSwapDelta::calls = 0;
    const auto evaluations = run_best_improvement(
        el::neighborhood_union(
            el::neighborhood<TwoOptExplorer>()
                | el::delta<TourLength, CountedTwoOptDelta>(),
            el::neighborhood<SwapExplorer>()
                | el::delta<TourLength, CountedSwapDelta>()));
    assert(evaluations > 0);
    assert(CountedTwoOptDelta::calls > 0);
    assert(CountedSwapDelta::calls > 0);
    // Every evaluation but the initial solution's is a delta.
    assert(CountedTwoOptDelta::calls + CountedSwapDelta::calls == evaluations - 1);

    // The swaps have no delta: the 2-opt delta is not called either, and every
    // move is evaluated in full.
    CountedTwoOptDelta::calls = 0;
    const auto full_evaluations = run_best_improvement(
        el::neighborhood_union(
            el::neighborhood<TwoOptExplorer>()
                | el::delta<TourLength, CountedTwoOptDelta>(),
            el::neighborhood<SwapExplorer>()));
    assert(full_evaluations > 0);
    assert(CountedTwoOptDelta::calls == 0);
    return 0;
}
