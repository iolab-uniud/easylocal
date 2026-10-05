// The parameters of cost components and cost::apply functions: a class whose
// parameters_type is a parameter block is built from it and configured under
// its name, cost.<name>.* in a runner.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/recipes.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/runner.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{

namespace config = easylocal::config;
namespace cost = easylocal::cost;
using easylocal::component;
using easylocal::solution_manager;

struct Line
{
    std::vector<int> values{3, 9, 5};
};

struct Picks
{
    std::vector<int> chosen{0, 1, 2};
};

class LineManager
{
public:
    using input_type = Line;
    using solution_type = Picks;

    explicit LineManager(const Line& line) : line_{line} {}

    [[nodiscard]] const Line& input() const noexcept
    {
        return line_;
    }

    [[nodiscard]] static bool is_valid(const Picks&) noexcept
    {
        return true;
    }

private:
    const Line& line_;
};

struct PickParameters
{
    int picks{3};

    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"picks", &PickParameters::picks>(
                "Values picked by the initial solution",
                config::range(0, 3)));
    }

    [[nodiscard]] config::validation_result validate() const
    {
        return config::check_schema(*this);
    }
};

// A SolutionManager with parameters: how many values its initial solution
// picks.
class PickingManager
{
public:
    using input_type = Line;
    using solution_type = Picks;
    using parameters_type = PickParameters;

    PickingManager(const Line& line, const PickParameters& parameters)
        : line_{line}, picks_{parameters.picks}
    {
    }

    [[nodiscard]] const Line& input() const noexcept
    {
        return line_;
    }

    [[nodiscard]] static bool is_valid(const Picks&) noexcept
    {
        return true;
    }

    [[nodiscard]] Picks initial_solution() const
    {
        Picks result;
        result.chosen.resize(static_cast<std::size_t>(picks_));
        for (std::size_t index = 0; index < result.chosen.size(); ++index)
            result.chosen[index] = static_cast<int>(index);
        return result;
    }

private:
    const Line& line_;
    int picks_;
};

struct ThresholdParameters
{
    int threshold{4};

    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"threshold", &ThresholdParameters::threshold>(
                "Values above it count",
                config::range(0, 100)));
    }

    [[nodiscard]] config::validation_result validate() const
    {
        return config::check_schema(*this);
    }
};

// The number of picked values above a threshold.
class AboveThreshold
{
public:
    using parameters_type = ThresholdParameters;

    AboveThreshold(const Line& line, const ThresholdParameters& parameters)
        : line_{line}, threshold_{parameters.threshold}
    {
    }

    [[nodiscard]] static std::string_view name() noexcept
    {
        return "above";
    }

    [[nodiscard]] int evaluate(const Picks& picks) const
    {
        return static_cast<int>(
            std::ranges::count_if(picks.chosen, [this](const int index) {
                return line_.values[index] > threshold_;
            }));
    }

private:
    const Line& line_;
    int threshold_;
};

struct Drop
{
    std::size_t position{};
};

// Drops one of the picks.
class DropExplorer : public easylocal::neighborhood_explorer_base<LineManager, Drop>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]] static bool is_valid(const Picks& picks, const Drop& drop) noexcept
    {
        return drop.position < picks.chosen.size();
    }

    [[nodiscard]] static std::vector<Drop> moves(const Picks& picks)
    {
        std::vector<Drop> result;
        for (std::size_t position = 0; position < picks.chosen.size(); ++position)
            result.push_back(Drop{position});
        return result;
    }

    static void make_move(Picks& picks, const Drop& drop)
    {
        picks.chosen.erase(
            picks.chosen.begin() + static_cast<std::ptrdiff_t>(drop.position));
    }
};

// The same count, under the same name.
class AboveAgain : public AboveThreshold
{
public:
    using AboveThreshold::AboveThreshold;
};

// The number of picked values.
class Count
{
public:
    [[nodiscard]] static int evaluate(const Picks& picks)
    {
        return static_cast<int>(picks.chosen.size());
    }
};

// The sum of the picked values.
class Total
{
public:
    explicit Total(const Line& line) : line_{line} {}

    [[nodiscard]] int evaluate(const Picks& picks) const
    {
        int total = 0;
        for (const auto index : picks.chosen)
            total += line_.values[index];
        return total;
    }

private:
    const Line& line_;
};

struct ExcessParameters
{
    int bound{10};

    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"bound", &ExcessParameters::bound>(
                "The largest total without excess",
                config::range(0, 1000)));
    }

    [[nodiscard]] config::validation_result validate() const
    {
        return config::check_schema(*this);
    }
};

// How much a total exceeds a bound.
class Excess
{
public:
    using parameters_type = ExcessParameters;

    explicit Excess(const ExcessParameters& parameters) : bound_{parameters.bound} {}

    [[nodiscard]] static std::string_view name() noexcept
    {
        return "excess";
    }

    [[nodiscard]] int operator()(const int total) const noexcept
    {
        return std::max(0, total - bound_);
    }

private:
    int bound_;
};

[[nodiscard]]
bool has_parameter(
    const config::parameter_set& parameters,
    const std::string_view path,
    const std::string_view value)
{
    return std::ranges::any_of(
        parameters.parameters(),
        [&](const config::parameter_info& parameter) {
            return parameter.path == path && parameter.value == value;
        });
}

bool expect(const bool condition, const std::string_view description)
{
    if (!condition)
        std::cerr << "FAILED: " << description << '\n';
    return condition;
}

} // namespace

int main()
{
    bool ok = true;
    const Line line;
    const Picks picks;

    {
        // A component with parameters, given and then changed by name.
        auto recipe = solution_manager<LineManager>()
            | component<AboveThreshold>(ThresholdParameters{.threshold = 4});
        ok &= expect(
            recipe.construct(line).evaluate(picks) == 2,
            "a component is built from the parameters of its recipe");
        auto parameters = recipe.configuration();
        ok &= expect(
            has_parameter(parameters, "cost.above.threshold", "4"),
            "a component's parameters are under its name");
        const std::array change{config::text_override{"cost.above.threshold", "6"}};
        ok &= expect(
            static_cast<bool>(parameters.apply(change))
                && recipe.construct(line).evaluate(picks) == 1,
            "a changed parameter builds the component again");
        const std::array invalid{config::text_override{"cost.above.threshold", "200"}};
        ok &=
            expect(!parameters.apply(invalid), "a component's parameters are validated");
    }

    {
        // A function with parameters, deep in the expression: still under its
        // name, next to the weights, which keep their place.
        auto recipe = solution_manager<LineManager>()
            | cost::hard_soft(
                cost::apply<Excess>({.bound = 10}, component<Total>()),
                cost::sum(component<Count>() * 2, component<AboveThreshold>()));
        ok &= expect(
            recipe.construct(line).evaluate(picks) == cost::hierarchical<int, int>{7, 8},
            "a function is built from its parameters");
        auto parameters = recipe.configuration();
        ok &= expect(
            has_parameter(parameters, "cost.excess.bound", "10")
                && has_parameter(parameters, "cost.above.threshold", "4")
                && has_parameter(parameters, "cost.soft.weights", "[2, 1]"),
            "functions and components by name, weights by place");
        const std::array change{config::text_override{"cost.excess.bound", "15"}};
        ok &= expect(
            static_cast<bool>(parameters.apply(change))
                && recipe.construct(line).evaluate(picks).hard() == 2,
            "a changed parameter builds the function again");
    }

    {
        // In a runner, under cost.
        auto runner = easylocal::make_runner<easylocal::runners::FirstImprovement>({})
            | (solution_manager<LineManager>()
                | cost::apply<Excess>({.bound = 12}, component<Total>()))
            | easylocal::neighborhood<DropExplorer>();
        const auto parameters = runner.configuration();
        ok &= expect(
            has_parameter(parameters, "cost.excess.bound", "12"),
            "a runner gives a function's parameters as cost.<name>");
    }

    {
        // A SolutionManager with parameters, at the root solution_manager.
        auto recipe = solution_manager<PickingManager>(PickParameters{.picks = 2})
            | cost::sum(component<Total>(), component<AboveThreshold>());
        ok &= expect(
            recipe.construct(line).base().initial_solution().chosen.size() == 2,
            "a SolutionManager is built from the parameters of its recipe");
        auto parameters = recipe.configuration();
        ok &= expect(
            has_parameter(parameters, "solution_manager.picks", "2")
                && has_parameter(parameters, "cost.weights", "[1, 1]"),
            "a SolutionManager's parameters are under solution_manager");
        const std::array change{config::text_override{"solution_manager.picks", "1"}};
        ok &= expect(
            static_cast<bool>(parameters.apply(change))
                && recipe.construct(line).base().initial_solution().chosen.size() == 1,
            "a changed parameter builds the SolutionManager again");
        auto runner = easylocal::make_runner<easylocal::runners::FirstImprovement>({})
            | recipe | easylocal::neighborhood<DropExplorer>();
        ok &= expect(
            has_parameter(runner.configuration(), "solution_manager.picks", "1"),
            "a runner gives them at its root");
    }

    {
        // Two configurable leaves with the same name collide.
        auto recipe = solution_manager<LineManager>()
            | cost::sum(
                component<AboveThreshold>(),
                cost::apply<Excess>({}, component<AboveAgain>()));
        bool threw = false;
        try
        {
            static_cast<void>(recipe.configuration());
        }
        catch (const std::invalid_argument&)
        {
            threw = true;
        }
        ok &= expect(threw, "two components under the same name are rejected");
    }

    return ok ? 0 : 1;
}
