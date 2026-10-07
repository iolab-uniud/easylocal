// Every parameter of the built-in blocks declares its domain: a range, one_of,
// or easylocal::unlimited for any value (booleans have true and false). A new
// parameter without one fails this test, as check(app) fails for an app. The
// rules between parameters are requirements of the schemas, which an irace
// scenario writes as forbidden configurations. A real parameter with no upper
// bound excludes infinity.
#include <easylocal/app/cli.hpp>
#include <easylocal/app/run_parameters.hpp>
#include <easylocal/app/tuning.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/helpers/detail/cost_expression.hpp>
#include <easylocal/helpers/neighborhood_union.hpp>
#include <easylocal/runners/best_improvement.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/great_deluge.hpp>
#include <easylocal/runners/hill_climbing.hpp>
#include <easylocal/runners/late_acceptance_hill_climbing.hpp>
#include <easylocal/runners/pareto_late_acceptance_hill_climbing.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/runners/tabu_search.hpp>
#include <easylocal/solvers/multi_start.hpp>
#include <easylocal/solvers/pipeline.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace
{

namespace config = easylocal::config;
namespace runners = easylocal::runners;
namespace sa = easylocal::runners::temperature;
namespace tabu = easylocal::runners::tabu;

std::vector<std::string> missing;

template<class Block>
void expect_domains(const std::string& name)
{
    Block block{};
    config::parameter_set set;
    set.add(name, block);
    for (auto& path : config::undeclared_domains(set))
        missing.push_back(std::move(path));
}

std::vector<std::string> unlisted;

// A block that breaks a rule between its parameters breaks a requirement of its
// schema, not only its validate().
template<class Block>
void expect_requirement(const std::string& name, Block block)
{
    config::parameter_set set;
    set.add(name, block);
    const auto requirements = set.requirements();
    if (std::ranges::none_of(requirements, [](const config::requirement_info& rule) {
            return !rule.satisfied;
        }))
        unlisted.push_back(name);
}

std::vector<std::string> infinite;

// A block whose real parameter `field` is infinite is out of the domain of
// that field, which excludes infinity: the block and a configuration name it.
template<class Block, class Set>
void expect_finite(
    const std::string& name,
    const std::string_view field,
    Set set_infinite)
{
    Block block{};
    set_infinite(block, std::numeric_limits<double>::infinity());
    const auto validation = block.validate();
    const std::string expected = std::string{field} + ": expected a value in";
    bool rejected = !validation && validation.message.contains(expected);
    config::parameter_set set;
    set.add(name, block);
    const auto diagnostics = set.validate().diagnostics;
    rejected = rejected
        && std::ranges::any_of(
            diagnostics,
            [&](const config::parameter_diagnostic& diagnostic) {
                return diagnostic.path.ends_with(field)
                    && diagnostic.message.starts_with("expected a value in");
            });
    if (!rejected)
        infinite.push_back(name + "." + std::string{field});
}

template<class... Lists>
void expect_tabu_searches()
{
    (expect_domains<runners::TabuSearchParameters<Lists>>("tabu_search"), ...);
    (expect_domains<runners::TabuSearchParameters<
            Lists,
            runners::candidates::FirstImprovementParameters>>(
         "first_improvement_tabu_search"),
        ...);
    (expect_domains<runners::TabuSearchParameters<
            Lists,
            runners::candidates::AspirationPlusParameters>>(
         "aspiration_plus_tabu_search"),
        ...);
    (expect_domains<
         runners::TabuSearchParameters<Lists, runners::candidates::EliteListParameters>>(
         "elite_candidate_tabu_search"),
        ...);
}

} // namespace

int main()
{
    expect_domains<runners::FirstImprovementParameters>("first_improvement");
    expect_domains<runners::BestImprovementParameters>("best_improvement");
    expect_domains<runners::HillClimbingParameters>("hill_climbing");
    expect_domains<runners::LateAcceptanceHillClimbingParameters>("late_acceptance");
    expect_domains<runners::ParetoLateAcceptanceHillClimbingParameters>(
        "pareto_late_acceptance");
    expect_domains<runners::GreatDelugeParameters>("great_deluge");

    expect_domains<runners::SimulatedAnnealingParameters<sa::ClassicParameters>>(
        "sa.classic");
    expect_domains<runners::SimulatedAnnealingParameters<sa::FixedLengthParameters>>(
        "sa.fixed_length");
    expect_domains<runners::SimulatedAnnealingParameters<sa::CutoffParameters>>(
        "sa.cutoff");
    expect_domains<runners::SimulatedAnnealingParameters<sa::FixedTemperatureParameters>>(
        "sa.fixed_temperature");
    expect_domains<runners::SimulatedAnnealingParameters<sa::TimeBasedParameters>>(
        "sa.time_based");
    expect_domains<sa::ReheatingParameters<sa::ClassicParameters>>("sa.reheating");

    expect_tabu_searches<
        tabu::FixedLengthParameters,
        tabu::RandomTenureParameters,
        tabu::CyclicParameters,
        tabu::ReactiveParameters,
        tabu::FrequencyParameters,
        tabu::ObjectiveBasedParameters,
        tabu::LimDynamicParameters,
        tabu::FooParameters,
        tabu::RandomFooParameters>();

    expect_domains<easylocal::solvers::MultiStartParameters>("multi_start");
    expect_domains<easylocal::solvers::StageParameters>("stage");
    expect_domains<easylocal::NeighborhoodUnionParameters<2>>("neighborhood");
    expect_domains<easylocal::detail::sum_parameters<double, 2>>("cost");

    expect_domains<easylocal::RunParameters>("run");
    expect_domains<easylocal::TuningParameters>("tuning");
    expect_domains<easylocal::cli::CommandLineParameters>("cli");

    expect_requirement(
        "random_tenure",
        tabu::RandomTenureParameters{.min_tenure = 5, .max_tenure = 4});
    expect_requirement(
        "lim_dynamic",
        tabu::LimDynamicParameters{.min_tenure = 5, .max_tenure = 5});
    expect_requirement(
        "random_foo.window",
        tabu::RandomFooParameters{.min_window = 5, .max_window = 4});
    expect_requirement(
        "random_foo.increment",
        tabu::RandomFooParameters{.min_increment = 5, .max_increment = 4});
    expect_requirement(
        "random_foo.fluctuation",
        tabu::RandomFooParameters{.min_fluctuation = 2.0, .max_fluctuation = 1.0});
    expect_requirement(
        "aspiration_plus",
        runners::TabuSearchParameters<
            tabu::FixedLengthParameters,
            runners::candidates::AspirationPlusParameters>{
            .candidates = {.min_moves = 5, .max_moves = 4}});
    expect_requirement(
        "reheating.temperature",
        sa::ReheatingParameters<sa::ClassicParameters>{
            .descent = {.initial_temperature = 8.0, .final_temperature = 6.0},
            .reheat_ratio = 0.5});
    expect_requirement(
        "reheating.budget",
        sa::ReheatingParameters<sa::FixedLengthParameters>{
            .descent = {.allowed_iterations = 10},
            .allowed_reheats = 3,
            .first_descent_share = 0.9});

    expect_finite<runners::GreatDelugeParameters>(
        "great_deluge",
        "initial_level",
        [](auto& block, const double value) { block.initial_level = value; });
    expect_finite<runners::GreatDelugeParameters>(
        "great_deluge",
        "min_level",
        [](auto& block, const double value) { block.min_level = value; });
    expect_finite<sa::ClassicParameters>(
        "classic",
        "initial_temperature",
        [](auto& block, const double value) { block.initial_temperature = value; });
    expect_finite<sa::FixedLengthParameters>(
        "fixed_length",
        "initial_temperature",
        [](auto& block, const double value) { block.initial_temperature = value; });
    expect_finite<sa::CutoffParameters>(
        "cutoff",
        "initial_temperature",
        [](auto& block, const double value) { block.initial_temperature = value; });
    expect_finite<sa::FixedTemperatureParameters>(
        "fixed_temperature",
        "temperature",
        [](auto& block, const double value) { block.temperature = value; });
    expect_finite<sa::TimeBasedParameters>(
        "time_based",
        "initial_temperature",
        [](auto& block, const double value) { block.initial_temperature = value; });
    expect_finite<sa::TimeBasedParameters>(
        "time_based",
        "allowed_running_time",
        [](auto& block, const double value) { block.allowed_running_time = value; });
    expect_finite<sa::ReheatingParameters<sa::ClassicParameters>>(
        "reheating",
        "reheat_ratio",
        [](auto& block, const double value) {
            block.allowed_reheats = 1;
            block.reheat_ratio = value;
        });
    expect_finite<sa::ReheatingParameters<sa::FixedLengthParameters>>(
        "reheating",
        "first_descent_share",
        [](auto& block, const double value) {
            block.allowed_reheats = 1;
            block.first_descent_share = value;
        });
    expect_finite<tabu::ReactiveParameters>(
        "reactive",
        "increase",
        [](auto& block, const double value) { block.increase = value; });
    expect_finite<tabu::FooParameters>(
        "foo",
        "fluctuation",
        [](auto& block, const double value) { block.fluctuation = value; });
    expect_finite<tabu::RandomFooParameters>(
        "random_foo",
        "max_fluctuation",
        [](auto& block, const double value) { block.max_fluctuation = value; });
    expect_finite<runners::candidates::AspirationPlusParameters>(
        "aspiration_plus",
        "aspiration_level",
        [](auto& block, const double value) { block.aspiration_level = value; });
    expect_finite<runners::candidates::EliteListParameters>(
        "elite_list",
        "quality",
        [](auto& block, const double value) { block.quality = value; });

    for (const auto& path : missing)
        std::cerr << path << " declares no domain\n";
    for (const auto& path : infinite)
        std::cerr << path << ": infinity is not out of its domain\n";
    for (const auto& name : unlisted)
        std::cerr << name << ": a rule between parameters is not a requirement\n";
    return missing.empty() && unlisted.empty() && infinite.empty()
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
