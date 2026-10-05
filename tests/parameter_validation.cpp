// Invalid parameters throw std::invalid_argument, with the path of the
// parameter, wherever they enter: a policy or an algorithm constructed from
// them, make_runner, the binding of a runner or of an app, and a Session.

#include <easylocal/app/app.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/hill_climbing.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/runners/tabu_search.hpp>

#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace
{

using namespace easylocal;
using namespace easylocal::runners;

struct ChainInstance
{
};

struct ChainSolution
{
    int value{};
};

struct ChainMove
{
};

class ChainSolutionManager : public solution_manager_base<ChainInstance, ChainSolution>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]]
    static bool is_valid(const ChainSolution&) noexcept
    {
        return true;
    }

    [[nodiscard]]
    static ChainSolution initial_solution() noexcept
    {
        return {};
    }
};

class ChainNeighborhood
    : public neighborhood_explorer_base<ChainSolutionManager, ChainMove>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    static std::optional<ChainMove> random_move(const ChainSolution& solution, RNG&)
    {
        if (solution.value >= 10)
            return std::nullopt;
        return ChainMove{};
    }

    [[nodiscard]]
    static bool is_valid(const ChainSolution&, const ChainMove&) noexcept
    {
        return true;
    }

    static void make_move(ChainSolution& solution, const ChainMove&) noexcept
    {
        ++solution.value;
    }
};

// The parameters of a neighborhood: a positive step.
struct StepParameters
{
    int step{1};

    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"step", &StepParameters::step>(
                "Step",
                config::range(1, easylocal::unlimited)));
    }

    [[nodiscard]]
    config::validation_result validate() const
    {
        return config::check_schema(*this);
    }
};

class SteppingNeighborhood : public ChainNeighborhood
{
public:
    using parameters_type = StepParameters;

    SteppingNeighborhood(const ChainSolutionManager& manager, const StepParameters&)
        : ChainNeighborhood{manager}
    {
    }
};

struct ChainCost
{
    [[nodiscard]]
    static int evaluate(const ChainSolution& solution) noexcept
    {
        return solution.value;
    }
};

[[nodiscard]]
auto chain_app(const HillClimbingParameters parameters)
{
    return easylocal::app("chain")
        .with_solution_manager(
            solution_manager<ChainSolutionManager>() | component<ChainCost>())
        .with_neighborhood(neighborhood<ChainNeighborhood>())
        .with_runner<HillClimbing>("hc", parameters);
}

bool expect(const bool condition, const std::string_view description)
{
    if (!condition)
        std::cerr << "FAILED: " << description << '\n';
    return condition;
}

// The message of the std::invalid_argument that make throws, or nothing when
// it throws none.
template<class Make>
std::optional<std::string> invalid_argument(Make&& make)
{
    try
    {
        make();
    }
    catch (const std::invalid_argument& error)
    {
        return std::string{error.what()};
    }
    return std::nullopt;
}

bool says(const std::optional<std::string>& message, const std::string_view text)
{
    if (message && message->find(text) == std::string::npos)
        std::cerr << "message: " << *message << '\n';
    return message && message->find(text) != std::string::npos;
}

} // namespace

int main()
{
    bool ok = true;

    static_assert(
        !std::is_nothrow_constructible_v<
            temperature::Classic,
            temperature::ClassicParameters>,
        "a policy that validates its parameters may throw");

    ok &= expect(
        says(
            invalid_argument([] {
                return temperature::Classic{
                    {.initial_temperature = 1.0, .final_temperature = 2.0}};
            }),
            "final_temperature must be smaller than initial_temperature"),
        "a temperature policy rejects its invalid parameters");
    ok &= expect(
        says(
            invalid_argument([] { return temperature::Classic{{.cooling_rate = 1.5}}; }),
            "cooling_rate: expected a value in"),
        "the message of an invalid field names the field");
    ok &= expect(
        says(
            invalid_argument([] {
                return tabu::RandomTenure{{.min_tenure = 10, .max_tenure = 5}};
            }),
            "max_tenure must not be below min_tenure"),
        "a tabu list rejects its invalid parameters");
    ok &= expect(
        says(
            invalid_argument([] { return TabuSearch<>{{.tabu_list = {.tenure = 0}}}; }),
            "tabu_list.tenure: expected a value in"),
        "Tabu Search names the invalid field of its list");
    ok &= expect(
        says(
            invalid_argument([] { return HillClimbing{{.max_idle_iterations = 0}}; }),
            "max_idle_iterations: expected a value in"),
        "an algorithm rejects its invalid parameters");
    ok &= expect(
        says(
            invalid_argument([] {
                return make_runner<SimulatedAnnealing<temperature::Classic>>(
                    {.temperature = {.cooling_rate = 2.0}});
            }),
            "temperature.cooling_rate: expected a value in"),
        "make_runner rejects invalid parameters, with the path of the group");
    ok &= expect(
        says(
            invalid_argument([] {
                return neighborhood<SteppingNeighborhood>(StepParameters{.step = 0});
            }),
            "step: expected a value in"),
        "a neighborhood recipe rejects invalid parameters");
    ok &= expect(
        !invalid_argument([] { return temperature::Classic{{}}; }).has_value(),
        "the default parameters are valid");

    {
        const ChainInstance instance;
        auto runner = make_runner<HillClimbing>()
            | (solution_manager<ChainSolutionManager>() | component<ChainCost>())
            | neighborhood<ChainNeighborhood>();
        runner.parameters().max_idle_iterations = 0;
        ok &= expect(
            says(
                invalid_argument([&] { return runner.bind(instance); }),
                "max_idle_iterations"),
            "binding a runner whose parameters were made invalid throws");
    }

    {
        const ChainInstance instance;
        const auto application = chain_app({.max_idle_iterations = 0});
        ok &= expect(
            says(
                invalid_argument([&] { return application.bind(instance); }),
                "runners.hc.max_idle_iterations: expected a value in"),
            "binding an app with invalid parameters throws, with their path");
        ok &= expect(
            says(
                invalid_argument([&] { return Session{application}; }),
                "runners.hc.max_idle_iterations"),
            "a Session on an app with invalid parameters is not made");
        ok &= expect(
            !invalid_argument([] { return Session{chain_app({})}; }).has_value(),
            "a Session on a valid app is made");
    }

    return ok ? 0 : 1;
}
