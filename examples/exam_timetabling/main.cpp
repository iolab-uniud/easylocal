#include "cost_components.hpp"
#include "cost_deltas.hpp"
#include "instance_io.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/config/tree.hpp>
#include <easylocal/runner.hpp>
#include <easylocal/search/simulated_annealing.hpp>
#include <easylocal/search/temperature_policy.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>

#ifndef EASYLOCAL_EXAM_MWE_INSTANCE_FILE
#error "EASYLOCAL_EXAM_MWE_INSTANCE_FILE must name the example instance"
#endif

namespace
{

using namespace easylocal::mwe::exam_timetabling;

struct AppParameters
{
    std::filesystem::path instance_file;
    std::uint64_t seed{2026U};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<
                "instance_file",
                &AppParameters::instance_file>(
                    "Exam-timetabling instance file"),
            easylocal::config::field<
                "seed",
                &AppParameters::seed>(
                    "Pseudo-random generator seed"));
    }

    [[nodiscard]]
    auto validate() const noexcept -> easylocal::config::validation_result
    {
        if (instance_file.empty())
        {
            return easylocal::config::validation_result::failure(
                "instance_file must not be empty");
        }

        return easylocal::config::validation_result::success();
    }
};

template<class Parameters>
void require_valid(const Parameters& parameters)
{
    const auto validation = parameters.validate();
    if (!validation)
    {
        throw std::invalid_argument{std::string{validation.message}};
    }
}

template<class Tree>
void print_configuration(const Tree& tree)
{
    std::cout << "configuration:\n";
    easylocal::config::for_each_config_parameter(
        tree,
        [](const auto path, const auto, const auto&) {
            using path_type = std::remove_cvref_t<decltype(path)>;
            bool first = true;
            std::cout << "  ";
            for (const auto segment : path_type::segments())
            {
                if (!first)
                {
                    std::cout << '.';
                }
                std::cout << segment;
                first = false;
            }
            std::cout << '\n';
        });
}

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

int main()
{
    using namespace easylocal::mwe::exam_timetabling;
    using easylocal::Runner;
    using easylocal::component;
    using easylocal::delta;
    using easylocal::neighborhood;
    using easylocal::solution_manager;
    using easylocal::search::SimulatedAnnealing;
    using easylocal::search::temperature::FixedLength;
    using easylocal::search::temperature::FixedLengthParameters;

    try
    {
        AppParameters app_parameters{
            .instance_file = EASYLOCAL_EXAM_MWE_INSTANCE_FILE,
            .seed = 2026U,
        };
        FixedLengthParameters temperature_parameters{
            .initial_temperature = 100.0,
            .final_temperature = 1.0,
            .cooling_rate = 0.5,
            .max_iterations = 30,
        };

        require_valid(app_parameters);
        require_valid(temperature_parameters);

        auto runner =
            Runner{SimulatedAnnealing{FixedLength{temperature_parameters}}}
            | (solution_manager<ExamTimetablingSolutionManager>()
               | component<StudentConflictComponent>()
               | component<ConsecutiveExamComponent>()
               | component<TimeslotLoadComponent>())
            | (neighborhood<MoveExamNeighborhoodExplorer>()
               | delta<
                     StudentConflictComponent,
                     StudentConflictDeltaEvaluator>()
               | delta<
                     ConsecutiveExamComponent,
                     ConsecutiveExamDeltaEvaluator>()
               | delta<
                     TimeslotLoadComponent,
                     TimeslotLoadDeltaEvaluator>());

        const auto configuration = easylocal::config::root(
            easylocal::config::named<"application">(app_parameters),
            runner.configuration<"solver">());
        print_configuration(configuration);

        const auto instance = load_instance(app_parameters.instance_file);
        const ExamTimetable initial_solution{
            .timeslot_by_exam = {0, 0, 1, 2},
        };

        std::mt19937 rng{static_cast<std::mt19937::result_type>(
            app_parameters.seed)};
        const auto result = runner.bind(instance).run(initial_solution, rng);

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
