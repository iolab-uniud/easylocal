// A runner registered with its own neighborhood is checked at registration:
// First Improvement enumerates moves, which a random-only neighborhood lacks.
#include "../../examples/tutorial/tsp.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/runners/first_improvement.hpp>

#include <optional>
#include <random>

namespace
{

class RandomSwapExplorer
    : public easylocal::neighborhood_explorer_base<
          tutorial::TourManager,
          tutorial::SwapCities>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    template<std::uniform_random_bit_generator RNG>
    std::optional<tutorial::SwapCities> random_move(const tutorial::Tour&, RNG&) const
    {
        return tutorial::SwapCities{0, 1};
    }

    bool is_valid(const tutorial::Tour&, const tutorial::SwapCities&) const
    {
        return true;
    }

    void make_move(tutorial::Tour&, const tutorial::SwapCities&) const {}
};

} // namespace

int main()
{
    [[maybe_unused]] auto application = easylocal::app("tsp")
        | (easylocal::solution_manager<tutorial::TourManager>()
            | easylocal::component<tutorial::TourLength>())
        | easylocal::neighborhood<tutorial::TwoOptExplorer>()
        | easylocal::runner<easylocal::runners::FirstImprovement>(
            "random",
            {},
            easylocal::neighborhood<RandomSwapExplorer>());
}
