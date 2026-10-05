// EASYLOCAL_VERIFY_DELTAS re-evaluates the solution after every committed move
// and stops the program when a delta disagrees with the full evaluation. With
// the argument "wrong", the run uses a delta that is off by one, and the
// program must stop with the message of the check.
#include "../examples/tutorial/tsp.hpp"

#include <easylocal/helpers/recipes.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/runner.hpp>

#include <csignal>
#include <cstdlib>
#include <string_view>

namespace
{

// The 2-opt delta of the tutorial, plus one: wrong after every move.
class WrongTwoOptDelta
{
public:
    explicit WrongTwoOptDelta(const tutorial::Tsp& input) : correct_{input} {}

    [[nodiscard]] double delta_evaluate(
        const tutorial::Tour& tour,
        const tutorial::TwoOpt& move) const
    {
        return correct_.delta_evaluate(tour, move) + 1.0;
    }

private:
    tutorial::TwoOptLengthDelta correct_;
};

template<class Delta>
int descend()
{
    namespace el = easylocal;
    const auto tsp = tutorial::five_cities();
    auto runner = el::make_runner<el::runners::FirstImprovement>({})
        | (el::solution_manager<tutorial::TourManager>()
            | el::component<tutorial::TourLength>())
        | (el::neighborhood<tutorial::TwoOptExplorer>()
            | el::delta<tutorial::TourLength, Delta>());
    auto bound = runner.bind(tsp);
    const auto result = bound.run(bound.initial_solution());
    return result.cost == 26.0 ? 0 : 1;
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc > 1 && std::string_view{argv[1]} == "wrong")
    {
        // The check aborts; the test sees an exit status of 1.
        std::signal(SIGABRT, [](int) { std::_Exit(1); });
        return descend<WrongTwoOptDelta>();
    }
    return descend<tutorial::TwoOptLengthDelta>();
}
