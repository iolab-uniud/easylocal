// A copied or moved neighborhood union evaluates with the deltas of its own
// children: it builds its delta bindings once, and they point to the children
// it holds, not to those of the object it was copied from.
#include "../examples/tutorial/tsp.hpp"

#include <easylocal/helpers/neighborhood_union.hpp>
#include <easylocal/helpers/recipes.hpp>

#include <cassert>
#include <cstddef>
#include <ranges>
#include <utility>

namespace
{

using namespace tutorial;
namespace el = easylocal;

// The 2-opt delta of the tutorial, each instance with a serial of its own: a
// copy gets a new one, so the serial recorded by a call names the instance
// that ran, and so the union whose children it belongs to.
class SerialTwoOptDelta
{
public:
    static inline std::size_t created = 0;
    static inline std::size_t last_called = 0;

    explicit SerialTwoOptDelta(const Tsp& input) : delta_{input}, serial_{++created} {}

    SerialTwoOptDelta(const SerialTwoOptDelta& other)
        : delta_{other.delta_}, serial_{++created}
    {
    }

    SerialTwoOptDelta& operator=(const SerialTwoOptDelta& other)
    {
        if (this != &other)
        {
            delta_ = other.delta_;
            serial_ = ++created;
        }

        return *this;
    }

    ~SerialTwoOptDelta() = default;

    [[nodiscard]]
    double delta_evaluate(const Tour& tour, const TwoOpt& move) const
    {
        last_called = serial_;
        return delta_.delta_evaluate(tour, move);
    }

private:
    TwoOptLengthDelta delta_;
    std::size_t serial_;
};

// The first move of the union on the tour, which is a 2-opt move: the 2-opt
// child comes first.
[[nodiscard]]
auto first_move(const auto& explorer, const Tour& tour)
{
    auto range = el::moves(explorer, tour);
    auto move = *std::ranges::begin(range);
    return move;
}

} // namespace

int main()
{
    const auto input = five_cities();
    const auto manager =
        (el::solution_manager<TourManager>() | el::component<TourLength>())
            .construct(input);
    const auto tour = manager.initial_solution();

    const auto spec = el::neighborhood_union(
        el::neighborhood<TwoOptExplorer>() | el::delta<TourLength, SerialTwoOptDelta>(),
        el::neighborhood<SwapExplorer>());

    auto explorer = spec.construct(manager);
    const auto move = first_move(explorer, tour);
    const auto& binding = std::get<0>(explorer.delta_bindings());
    const auto expected = binding.try_apply(0.0, tour, move);
    assert(expected.has_value());

    // The copy evaluates with its own delta: the serial it records is not the
    // one of the delta the original holds.
    {
        auto copy = explorer;
        const auto& copied_binding = std::get<0>(copy.delta_bindings());
        SerialTwoOptDelta::last_called = 0;
        const auto value = copied_binding.try_apply(0.0, tour, move);
        assert(value.has_value());
        assert(*value == *expected);
        const auto called = SerialTwoOptDelta::last_called;

        SerialTwoOptDelta::last_called = 0;
        [[maybe_unused]] const auto original = binding.try_apply(0.0, tour, move);
        assert(SerialTwoOptDelta::last_called != 0);
        assert(called != SerialTwoOptDelta::last_called);
    }

    // The same of a move: the bindings of the moved-to union point to the
    // children it now holds, whose delta is a copy of the source's (the class
    // has no move constructor), with a serial of its own.
    {
        auto source = spec.construct(manager);
        SerialTwoOptDelta::last_called = 0;
        [[maybe_unused]] const auto before =
            std::get<0>(source.delta_bindings()).try_apply(0.0, tour, move);
        const auto source_serial = SerialTwoOptDelta::last_called;

        auto moved = std::move(source);
        SerialTwoOptDelta::last_called = 0;
        const auto value = std::get<0>(moved.delta_bindings()).try_apply(0.0, tour, move);
        assert(value.has_value());
        assert(*value == *expected);
        assert(SerialTwoOptDelta::last_called != 0);
        assert(SerialTwoOptDelta::last_called != source_serial);
    }

    return 0;
}
