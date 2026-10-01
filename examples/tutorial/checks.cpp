// Component contract checks for the tutorial's running example.
#include "tsp.hpp"

#include <easylocal/testing.hpp>

namespace
{

// [fixtures] ---------------------------------------------------------------
struct TspCheckData
{
    static auto instance() -> tutorial::Tsp { return tutorial::five_cities(); }
    static auto solution(const tutorial::Tsp&) -> tutorial::Tour
    {
        return tutorial::Tour{{0, 1, 2, 3, 4}};
    }
};

struct TourManagerCheck : TspCheckData
{
    using solution_manager = tutorial::TourManager;
};

struct TourLengthCheck : TspCheckData
{
    using solution_manager = tutorial::TourManager;
    using component = tutorial::TourLength;
};

struct TwoOptCheck : TspCheckData
{
    using neighborhood = tutorial::TwoOptExplorer;
};

struct TwoOptDeltaCheck : TspCheckData
{
    using neighborhood = tutorial::TwoOptExplorer;
    using component = tutorial::TourLength;
    using delta_evaluator = tutorial::TwoOptLengthDelta;
};
// [fixtures] ---------------------------------------------------------------

} // namespace

int main()
{
    // [run-checks]
    return easylocal::testing::run_checks(
        easylocal::testing::check_solution_manager<TourManagerCheck>(),
        easylocal::testing::check_cost_component<TourLengthCheck>(),
        easylocal::testing::check_neighborhood<TwoOptCheck>(),
        easylocal::testing::check_delta_evaluator<TwoOptDeltaCheck>());
    // [run-checks]
}
