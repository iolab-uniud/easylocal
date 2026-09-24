#pragma once

#include "cost.hpp"
#include "instance.hpp"
#include "solution.hpp"

#include <cassert>
#include <concepts>
#include <functional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::mwe::assignment
{

template<class Aggregator, class... Components>
class ComposedSolutionManager
{
public:
    static_assert(sizeof...(Components) > 0);

    using instance_type = Instance;
    using solution_type = Solution;
    using cost_type = std::invoke_result_t<
        const Aggregator&,
        typename Components::value_type...>;

    explicit ComposedSolutionManager(const Instance& instance) noexcept(
        std::is_nothrow_default_constructible_v<Aggregator> &&
        (std::is_nothrow_constructible_v<Components, const Instance&> && ...))
        requires std::default_initializable<Aggregator>
        : instance_{instance},
          aggregator_{},
          components_{Components{instance}...}
    {
    }

    ComposedSolutionManager(
        const Instance& instance,
        Aggregator aggregator) noexcept(
        std::is_nothrow_move_constructible_v<Aggregator> &&
        (std::is_nothrow_constructible_v<Components, const Instance&> && ...))
        : instance_{instance},
          aggregator_{std::move(aggregator)},
          components_{Components{instance}...}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]]
    auto is_valid(const Solution& solution) const noexcept -> bool
    {
        if (solution.assignment.size() != instance_.demand.size())
        {
            return false;
        }

        return std::ranges::all_of(
            solution.assignment,
            [this](const machine_id machine) {
                return machine < instance_.capacity.size();
            });
    }

    [[nodiscard]]
    auto evaluate(const Solution& solution) const -> cost_type
    {
        assert(is_valid(solution));

        return std::apply(
            [&](const auto&... component) {
                return std::invoke(
                    aggregator_,
                    component.evaluate(solution)...);
            },
            components_);
    }

private:
    const Instance& instance_;
    [[no_unique_address]] Aggregator aggregator_;
    std::tuple<Components...> components_;
};

using SolutionManager = ComposedSolutionManager<
    AssignmentCostAggregator,
    CapacityCostComponent>;

} // namespace easylocal::mwe::assignment
