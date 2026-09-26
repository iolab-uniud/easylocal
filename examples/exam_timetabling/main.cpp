#include "cost_components.hpp"
#include "cost_deltas.hpp"
#include "instance_io.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/aggregation.hpp>
#include <easylocal/config/cli.hpp>
#include <easylocal/config/file.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/config/tree.hpp>
#include <easylocal/runner.hpp>
#include <easylocal/search/simulated_annealing.hpp>
#include <easylocal/search/temperature_policy.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <random>
#include <span>
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

int main(int argc, char* argv[])
{
    using namespace easylocal::mwe::exam_timetabling;
    using easylocal::make_neighborhood_explorer;
    using easylocal::make_runner;
    using easylocal::make_solution_manager;
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

        auto sm =
            make_solution_manager<ExamTimetablingSolutionManager>()
                .with_component<StudentConflictComponent>()
                .with_component<ConsecutiveExamComponent>()
                .with_component<TimeslotLoadComponent>()
                .with_aggregator(easylocal::aggregation::weighted_sum{
                    penalty_type{1000}, penalty_type{10}, penalty_type{1}});

        auto nhe =
            make_neighborhood_explorer<MoveExamNeighborhoodExplorer>()
                .with_delta<StudentConflictComponent>()
                .with_delta<
                    ConsecutiveExamComponent,
                    ConsecutiveExamDeltaEvaluator>()
                .with_delta<
                    TimeslotLoadComponent,
                    TimeslotLoadDeltaEvaluator>();

        auto runner =
            make_runner<easylocal::runner::simulated_annealing>(
                FixedLength{temperature_parameters})
                .with_solution_manager(sm)
                .with_neighborhood(nhe);

        const auto configuration = easylocal::config::root(
            easylocal::config::named<"application">(app_parameters),
            runner.configuration<"solver">());

        const auto cli = easylocal::config::parse_cli(argc, argv);
        if (cli.help_requested)
        {
            std::cout << easylocal::config::cli_help(argv[0], configuration);
            return 0;
        }

        easylocal::config::config_file_parse_result file_configuration{};
        if (cli.config_file.has_value())
        {
            file_configuration =
                easylocal::config::load_config_file(*cli.config_file);
        }

        if (!cli || !file_configuration)
        {
            for (const auto& diagnostic : cli.diagnostics)
            {
                std::cerr << "error: " << diagnostic.argument << ": "
                          << diagnostic.message << '\n';
            }
            for (const auto& diagnostic : file_configuration.diagnostics)
            {
                std::cerr << "error: ";
                if (diagnostic.line != 0)
                {
                    std::cerr << "config line " << diagnostic.line << ": ";
                }
                std::cerr << diagnostic.message;
                if (!diagnostic.text.empty())
                {
                    std::cerr << " ('" << diagnostic.text << "')";
                }
                std::cerr << '\n';
            }
            return 2;
        }

        const auto effective_overrides = easylocal::config::overlay_overrides(
            std::span<const easylocal::config::owned_text_override>{
                file_configuration.overrides},
            std::span<const easylocal::config::text_override>{cli.overrides});
        const auto effective_views = easylocal::config::override_views(
            std::span<const easylocal::config::owned_text_override>{
                effective_overrides});
        const auto overrides = easylocal::config::apply_overrides(
            configuration,
            std::span<const easylocal::config::text_override>{effective_views});
        if (!overrides)
        {
            for (const auto& diagnostic : overrides.diagnostics)
            {
                std::cerr << "error: " << diagnostic.path;
                if (!diagnostic.value.empty())
                {
                    std::cerr << " = '" << diagnostic.value << '\'';
                }
                std::cerr << ": " << diagnostic.message << '\n';
            }
            return 2;
        }

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
