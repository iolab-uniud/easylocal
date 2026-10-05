// The randomized contract checks draw from a seeded pseudo-random generator:
// the tutorial's explorers, whose random_move() rejects draws until they fit,
// end on 40 cities, and the random solutions differ from each other.
#include "../examples/tutorial/tsp.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/app/check.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/testing.hpp>

#include <cstddef>
#include <iostream>
#include <random>
#include <set>
#include <vector>

namespace
{

using namespace tutorial;
namespace el = easylocal;
namespace elt = easylocal::testing;

// Symmetric integer distances, so that the deltas add up exactly.
Tsp cities(const std::size_t count)
{
    Tsp tsp;
    tsp.distance.assign(count, std::vector<double>(count));
    for (std::size_t a = 0; a < count; ++a)
    {
        for (std::size_t b = 0; b < count; ++b)
            if (a != b)
                tsp.distance[a][b] = static_cast<double>(1 + (a * b + a + b) % 97);
    }
    return tsp;
}

// The tours the check draws, to count the distinct ones.
std::set<std::vector<std::size_t>> drawn_tours;

class RecordingManager : public TourManager
{
public:
    using TourManager::TourManager;

    template<std::uniform_random_bit_generator RNG>
    Tour random_solution(RNG& rng) const
    {
        auto tour = TourManager::random_solution(rng);
        drawn_tours.insert(tour.order);
        return tour;
    }
};

} // namespace

int main()
{
    const elt::fixture<TourManager> tsp{cities(40)};

    const auto passed = elt::run_checks(
        std::cout,
        elt::check_neighborhood<SwapExplorer>(tsp),
        elt::check_neighborhood<TwoOptExplorer>(tsp),
        elt::check_delta_evaluator<TwoOptExplorer, TourLength, TwoOptLengthDelta>(tsp));
    if (passed != 0)
        return 1;

    // Each random solution comes from fresh draws.
    const elt::fixture<RecordingManager> recording{cities(40), {.random_samples = 16}};
    if (!elt::check_solution_manager(recording).passed() || drawn_tours.size() != 16)
        return 2;

    // Another seed draws other solutions.
    const elt::fixture<RecordingManager> reseeded{
        cities(40),
        {.random_samples = 16, .seed = 7}};
    if (!elt::check_solution_manager(reseeded).passed() || drawn_tours.size() != 32)
        return 3;

    const auto application = el::app("tsp-40")
        | (el::solution_manager<TourManager>() | el::component<TourLength>())
        | el::neighborhood<TwoOptExplorer>()
        | el::runner<el::runners::FirstImprovement>("fi");
    const auto instance = cities(40);
    if (!el::check(application, instance).passed())
        return 4;

    return 0;
}
