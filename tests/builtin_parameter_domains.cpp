// Every parameter of the built-in blocks declares its domain: a range, one_of,
// or easylocal::unlimited for any value (booleans have true and false). A new
// parameter without one fails this test, as check(app) fails for an app.
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

#include <cstdlib>
#include <iostream>
#include <string>
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

template<class... Lists>
void expect_tabu_searches()
{
    (expect_domains<runners::TabuSearchParameters<Lists>>("tabu_search"), ...);
    (expect_domains<runners::FirstImprovementTabuSearchParameters<Lists>>(
         "first_improvement_tabu_search"),
        ...);
    (expect_domains<runners::AspirationPlusTabuSearchParameters<Lists>>(
         "aspiration_plus_tabu_search"),
        ...);
    (expect_domains<runners::EliteCandidateTabuSearchParameters<Lists>>(
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
    expect_domains<easylocal::cli::parameters>("cli");

    for (const auto& path : missing)
        std::cerr << path << " declares no domain\n";
    return missing.empty() ? EXIT_SUCCESS : EXIT_FAILURE;
}
