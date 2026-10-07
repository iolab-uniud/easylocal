// A floating-point cost updated by deltas reaches its target: the hard cost
// 0.1 + 0.2, lowered by the deltas -0.1 and -0.2, ends at 2.8e-17 rather than
// 0, so the run re-evaluates in full a cost within the tolerance of the target
// before deciding that it is not reached.
#include "support/expect.hpp"

#include <easylocal/cost.hpp>
#include <easylocal/helpers/recipes.hpp>
#include <easylocal/runners/hill_climbing.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/solvers/pipeline.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <random>

namespace
{

struct Items
{
    std::array<double, 2> weights{0.1, 0.2};
};

struct Selection
{
    std::array<bool, 2> selected{true, true};
};

// Deselect one item.
struct Drop
{
    std::size_t item{};
};

class SelectionManager : public easylocal::solution_manager_base<Items, Selection>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]] static bool is_valid(const Selection&) noexcept
    {
        return true;
    }

    [[nodiscard]] static Selection initial_solution() noexcept
    {
        return {};
    }
};

// The weight of the selected items: a violation to bring to zero.
class Weight
{
public:
    explicit Weight(const Items& items) noexcept : items_{items} {}

    [[nodiscard]] double evaluate(const Selection& selection) const noexcept
    {
        double total = 0.0;
        for (std::size_t item = 0; item < selection.selected.size(); ++item)
            if (selection.selected[item])
                total += items_.weights[item];
        return total;
    }

    [[nodiscard]] double delta_evaluate(const Selection&, const Drop& drop) const noexcept
    {
        return -items_.weights[drop.item];
    }

private:
    const Items& items_;
};

struct Dropped
{
    [[nodiscard]] static int evaluate(const Selection& selection) noexcept
    {
        return static_cast<int>(!selection.selected[0])
            + static_cast<int>(!selection.selected[1]);
    }

    [[nodiscard]] static int delta_evaluate(const Selection&, const Drop&) noexcept
    {
        return 1;
    }
};

class DropNeighborhood
    : public easylocal::neighborhood_explorer_base<SelectionManager, Drop>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    template<class RNG>
    [[nodiscard]] static std::optional<Drop> random_move(
        const Selection& selection,
        RNG& rng)
    {
        if (!selection.selected[0] && !selection.selected[1])
            return std::nullopt;
        for (;;)
        {
            const Drop drop{std::uniform_int_distribution<std::size_t>{0, 1}(rng)};
            if (selection.selected[drop.item])
                return drop;
        }
    }

    [[nodiscard]] static bool is_valid(
        const Selection& selection,
        const Drop& drop) noexcept
    {
        return selection.selected[drop.item];
    }

    static void make_move(Selection& selection, const Drop& drop) noexcept
    {
        selection.selected[drop.item] = false;
    }
};

} // namespace

int main()
{
    using easylocal::component;
    using easylocal::delta;
    namespace cost = easylocal::cost;
    namespace solvers = easylocal::solvers;

    bool ok = true;
    const Items items;
    ok &= expect(
        0.1 + 0.2 - 0.1 - 0.2 > 0.0,
        "the deltas leave the weight above zero, by rounding");

    {
        auto runner = easylocal::make_runner<easylocal::runners::HillClimbing>()
            | (easylocal::solution_manager<SelectionManager>() | component<Weight>())
            | (easylocal::neighborhood<DropNeighborhood>() | delta<Weight>());
        std::mt19937 rng{1U};
        const auto result =
            runner.bind(items).run(Selection{}, rng, easylocal::stop_at(0.0));
        ok &= expect(
            result.termination == easylocal::termination_reason::target_reached
                && result.cost == 0.0,
            "a run reaches a zero target through deltas that drift above it");
    }

    {
        auto runner = easylocal::make_runner<easylocal::runners::HillClimbing>()
            | (easylocal::solution_manager<SelectionManager>()
                | cost::hard_soft(component<Weight>(), component<Dropped>()))
            | (easylocal::neighborhood<DropNeighborhood>() | delta<Weight>()
                | delta<Dropped>());
        auto pipeline =
            (solvers::stage("feasible", runner).until_feasible()
                | solvers::stage("full", runner))
                .seed(1);
        const auto result = pipeline.solve(items);
        ok &= expect(
            result.stages.front().termination
                    == easylocal::termination_reason::target_reached
                && result.cost.hard() == 0.0,
            "until_feasible() stops at a zero hard cost reached through deltas");
    }

    return ok ? 0 : 1;
}
