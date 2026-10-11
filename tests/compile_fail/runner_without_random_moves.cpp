// A runner registered on the app's neighborhood is checked at registration:
// Simulated Annealing draws random moves, which an enumerating neighborhood
// does not propose.
#include "../../examples/tutorial/tsp.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

#include <cstddef>
#include <utility>

namespace
{

class EnumeratingSwapExplorer
    : public easylocal::neighborhood_explorer_base<
          tutorial::TourManager,
          tutorial::SwapCities>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    easylocal::generator<tutorial::SwapCities> moves(const tutorial::Tour& tour) const
    {
        for (std::size_t i = 0; i + 1 < tour.order.size(); ++i)
            co_yield tutorial::SwapCities{i, i + 1};
    }

    bool is_valid(const tutorial::Tour&, const tutorial::SwapCities&) const
    {
        return true;
    }

    void make_move(tutorial::Tour& tour, const tutorial::SwapCities& move) const
    {
        std::swap(tour.order[move.i], tour.order[move.j]);
    }
};

} // namespace

int main()
{
    [[maybe_unused]] auto application = easylocal::app("tsp")
        | (easylocal::solution_manager<tutorial::TourManager>()
            | easylocal::component<tutorial::TourLength>())
        | easylocal::neighborhood<EnumeratingSwapExplorer>()
        | easylocal::runner<easylocal::runners::SimulatedAnnealing<>>("sa");
}
