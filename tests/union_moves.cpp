// The moves of a neighborhood union compare when its explorers' moves do, so a
// Session on a union offers the checks that compare moves.
#include "../examples/tutorial/tsp.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/helpers/neighborhood_union.hpp>
#include <easylocal/runners/first_improvement.hpp>

#include <cassert>
#include <concepts>

namespace
{

using namespace tutorial;
namespace el = easylocal;

struct Unequal
{
    int value{};
};

static_assert(std::equality_comparable<el::detail::tagged_neighborhood_move<0, TwoOpt>>);
static_assert(
    !std::equality_comparable<el::detail::tagged_neighborhood_move<0, Unequal>>);

[[nodiscard]] auto make_application()
{
    return el::app("union-tsp")
        | (el::solution_manager<TourManager>() | el::component<TourLength>())
        | (el::neighborhood_union(
               el::neighborhood<TwoOptExplorer>()
                   | el::delta<TourLength, TwoOptLengthDelta>(),
               el::neighborhood<SwapExplorer>())
            | el::random_biases(3.0, 1.0))
        | el::runner<el::runners::FirstImprovement>("fi");
}

} // namespace

int main()
{
    using session_type = el::Session<decltype(make_application())>;
    static_assert(std::equality_comparable<session_type::move_type>);
    static_assert(session_type::supports_move_independence_check);
    static_assert(session_type::supports_random_distribution_check);

    session_type session{make_application(), five_cities(), 1};
    session.use_initial_solution();
    const auto sampling = session.check_random_move_distribution(session.rng());
    assert(sampling.samples > 0);
    assert(sampling.out_of_neighborhood == 0);
    return 0;
}
