#include "cost_components.hpp"
#include "cost_deltas.hpp"
#include "instance_io.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/config/cli.hpp>
#include <easylocal/config/setup.hpp>
#include <easylocal/config/tree.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/runners/runner.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <random>

#ifndef EASYLOCAL_EXAM_MWE_INSTANCE_FILE
#error "EASYLOCAL_EXAM_MWE_INSTANCE_FILE must name the example instance"
#endif

namespace
{

using namespace exam_timetabling;

struct AppParameters
{
    std::filesystem::path instance_file;
    std::uint64_t seed{2026U};

    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"instance_file", &AppParameters::instance_file>(
                "Exam-timetabling instance file"),
            easylocal::config::field<"seed", &AppParameters::seed>(
                "Pseudo-random generator seed"));
    }

    easylocal::config::validation_result validate() const
    {
        if (instance_file.empty())
        {
            return easylocal::config::validation_result::failure(
                "instance_file must not be empty");
        }

        return easylocal::config::validation_result::success();
    }
};

void print_timetable(const ExamTimetable& solution)
{
    std::cout << '[';
    for (std::size_t exam = 0; exam < solution.timeslot_by_exam.size(); ++exam)
    {
        if (exam != 0)
        {
            std::cout << ", ";
        }
        std::cout << solution.timeslot_by_exam[exam];
    }
    std::cout << ']';
}

} // namespace

int main(int argc, char* argv[])
{
    using namespace exam_timetabling;
    using easylocal::component;
    using easylocal::runners::temperature::FixedLength;
    using easylocal::runners::temperature::FixedLengthParameters;

    try
    {
        AppParameters app_parameters{
            .instance_file = EASYLOCAL_EXAM_MWE_INSTANCE_FILE,
        };
        FixedLengthParameters temperature_parameters{
            .initial_temperature = 100.0,
            .final_temperature = 1.0,
            .cooling_rate = 0.5,
            .max_iterations = 30,
        };

        auto sm = easylocal::solution_manager<ExamTimetablingSolutionManager>().with_cost(
            easylocal::cost::sum(
                component<StudentConflictComponent>() * 1000,
                component<ConsecutiveExamComponent>() * 10,
                component<TimeslotLoadComponent>()));

        auto nhe =
            easylocal::neighborhood<MoveExamNeighborhoodExplorer>()
                .with_delta<StudentConflictComponent>()
                .with_delta<ConsecutiveExamComponent, ConsecutiveExamDeltaEvaluator>();
        // TimeslotLoadComponent has no delta (see cost_deltas.hpp): EasyLocal
        // re-evaluates it on a candidate solution.

        auto runner =
            easylocal::make_runner<easylocal::runners::SimulatedAnnealing<FixedLength>>(
                FixedLength{temperature_parameters})
                .with_solution_manager(sm)
                .with_neighborhood(nhe);

        const auto configuration = easylocal::config::root(
            easylocal::config::named<"application">(app_parameters),
            runner.configuration<"solver">());

        const auto configured =
            easylocal::config::load_and_apply(argc, argv, configuration);
        if (configured.help_requested)
        {
            std::cout << easylocal::config::cli_help(argv[0], configuration);
            return 0;
        }

        if (!configured)
        {
            easylocal::config::print_diagnostics(std::cerr, configured);
            return 2;
        }

        const auto instance = load_instance(app_parameters.instance_file);
        auto search = runner.bind(instance);
        const auto initial_solution = search.initial_solution();

        std::mt19937_64 rng{app_parameters.seed};
        const auto result = search.run(initial_solution, rng);

        std::cout << "instance:          " << app_parameters.instance_file << '\n';
        std::cout << "initial timetable: ";
        print_timetable(initial_solution);
        std::cout << '\n';
        std::cout << "best timetable:    ";
        print_timetable(result.solution);
        std::cout << '\n';
        std::cout << "best penalty: " << result.cost << '\n';
        std::cout << "iterations: " << result.iterations << '\n';
        std::cout << "evaluations: " << result.evaluations << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
