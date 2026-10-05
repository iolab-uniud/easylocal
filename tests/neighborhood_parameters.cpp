// A NeighborhoodExplorer with parameters: its recipe holds them, gives them to
// the explorer when a runner is bound, and exposes them under "neighborhood"
// (under "neighborhood.<position>" in a union).
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/helpers/neighborhood_union.hpp>
#include <easylocal/runners/best_improvement.hpp>
#include <easylocal/runners/runner.hpp>

#include <algorithm>
#include <array>
#include <iostream>
#include <optional>
#include <random>
#include <string_view>
#include <vector>

namespace
{

using namespace easylocal;

struct LineInstance
{
};

struct Position
{
    int value{};
};

struct Step
{
    int delta{};
};

class LineManager : public solution_manager_base<LineInstance, Position>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]] static auto is_valid(const Position&) noexcept -> bool
    {
        return true;
    }
};

struct StepParameters
{
    int stride{1};

    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"stride", &StepParameters::stride>("Step length"));
    }

    [[nodiscard]] auto validate() const -> config::validation_result
    {
        if (stride <= 0)
            return config::validation_result::failure("stride must be positive");
        return config::validation_result::success();
    }
};

// Steps up by the configured stride, up to a limit given as a recipe argument.
class StrideExplorer : public neighborhood_explorer_base<LineManager, Step>
{
public:
    using parameters_type = StepParameters;

    StrideExplorer(
        const LineManager& manager,
        const StepParameters& parameters,
        int limit = 100)
        : neighborhood_explorer_base{manager}, stride_{parameters.stride}, limit_{limit}
    {
    }

    [[nodiscard]] auto is_valid(const Position& position, const Step& step) const -> bool
    {
        return position.value + step.delta <= limit_;
    }

    [[nodiscard]] auto moves(const Position& position) const -> std::vector<Step>
    {
        if (!is_valid(position, Step{stride_}))
            return {};
        return {Step{stride_}};
    }

    static void make_move(Position& position, const Step& step) noexcept
    {
        position.value += step.delta;
    }

private:
    int stride_;
    int limit_;
};

// Without parameters: steps down by one.
class DownExplorer : public neighborhood_explorer_base<LineManager, Step>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]] static auto is_valid(const Position&, const Step&) -> bool
    {
        return true;
    }

    [[nodiscard]] static auto moves(const Position&) -> std::vector<Step>
    {
        return {Step{-1}};
    }

    static void make_move(Position& position, const Step& step) noexcept
    {
        position.value += step.delta;
    }
};

// Higher is better, and a delta for StrideExplorer, to check that attaching it
// keeps the parameters.
struct Height
{
    [[nodiscard]] static auto evaluate(const Position& position) noexcept -> int
    {
        return -position.value;
    }
};

struct HeightDelta
{
    explicit HeightDelta(const LineInstance&) {}

    [[nodiscard]] static auto delta_evaluate(const Position&, const Step& step) noexcept
        -> int
    {
        return -step.delta;
    }
};

static_assert(config::detail::parameterized<StrideExplorer>);
static_assert(!config::detail::parameterized<DownExplorer>);
static_assert(
    !config::detail::configuration_provider<decltype(neighborhood<DownExplorer>())>);

[[nodiscard]]
auto has_parameter(
    const config::parameter_set& parameters,
    const std::string_view path,
    const std::string_view value) -> bool
{
    return std::ranges::any_of(
        parameters.parameters(),
        [&](const config::parameter_info& parameter) {
            return parameter.path == path && parameter.value == value;
        });
}

auto expect(const bool condition, const std::string_view description) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }
    return true;
}

} // namespace

int main()
{
    bool ok = true;
    const LineInstance instance;
    const auto sm = solution_manager<LineManager>() | component<Height>();

    {
        auto recipe = neighborhood<StrideExplorer>();
        ok &= expect(
            recipe.parameters().stride == 1,
            "a recipe starts from the default parameters");
        auto given = neighborhood<StrideExplorer>(StepParameters{.stride = 3}, 10);
        ok &= expect(
            given.parameters().stride == 3,
            "a first argument of the parameters type gives the parameters");
        auto with_delta = std::move(given) | delta<Height, HeightDelta>();
        ok &= expect(
            with_delta.parameters().stride == 3,
            "attaching a delta keeps the parameters");
    }

    {
        auto runner = make_runner<runners::BestImprovement>({}) | sm
            | (neighborhood<StrideExplorer>(StepParameters{.stride = 3}, 10)
                | delta<Height, HeightDelta>());
        const auto first = runner.bind(instance).run(Position{0});
        ok &= expect(
            first.solution.value == 9,
            "the explorer is built with the recipe's parameters and arguments");

        auto configuration = runner.configuration();
        ok &= expect(
            has_parameter(configuration, "neighborhood.stride", "3"),
            "a runner exposes the explorer's parameters under neighborhood");
        const std::array invalid{config::text_override{"neighborhood.stride", "0"}};
        ok &= expect(
            !configuration.apply(invalid),
            "the explorer's parameters are validated");
        const std::array valid{config::text_override{"neighborhood.stride", "5"}};
        ok &= expect(
            static_cast<bool>(configuration.apply(valid))
                && runner.bind(instance).run(Position{0}).solution.value == 10,
            "a changed parameter applies from the next bind");
    }

    {
        auto runner = make_runner<runners::BestImprovement>({.max_evaluations = 3}) | sm
            | neighborhood_union(
                neighborhood<DownExplorer>(),
                neighborhood<StrideExplorer>(StepParameters{.stride = 2}));
        const auto configuration = runner.configuration();
        ok &= expect(
            has_parameter(configuration, "neighborhood.1.stride", "2")
                && std::ranges::none_of(
                    configuration.parameters(),
                    [](const config::parameter_info& parameter) {
                        return parameter.path.starts_with("neighborhood.0.");
                    }),
            "a union exposes its children's parameters under their positions");
    }

    return ok ? 0 : 1;
}
