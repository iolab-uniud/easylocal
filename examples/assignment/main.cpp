#include "capacity_delta.hpp"
#include "instance_io.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/config/tree.hpp>
#include <easylocal/runner.hpp>
#include <easylocal/search/first_improvement.hpp>

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

#ifndef EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE
#error "EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE must name the example instance"
#endif

namespace
{

using namespace easylocal::mwe::assignment;

struct AppParameters
{
    std::filesystem::path instance_file;

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<
                "instance_file",
                &AppParameters::instance_file>(
                    "Assignment instance file"));
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

void print_solution(const AssignmentSolution& solution)
{
    std::cout << '[';

    for (std::size_t job = 0; job < solution.assignment.size(); ++job)
    {
        if (job != 0)
        {
            std::cout << ", ";
        }

        std::cout << solution.assignment[job];
    }

    std::cout << ']';
}

} // namespace

int main()
{
    using namespace easylocal::mwe::assignment;
    using easylocal::Runner;
    using easylocal::component;
    using easylocal::delta;
    using easylocal::neighborhood;
    using easylocal::solution_manager;
    using easylocal::search::FirstImprovement;
    using easylocal::search::FirstImprovementParameters;
    using easylocal::search::FirstImprovementTermination;

    try
    {
        AppParameters app_parameters{
            .instance_file = EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE,
        };
        FirstImprovementParameters search_parameters{
            .max_evaluations = 100,
        };

        require_valid(app_parameters);
        require_valid(search_parameters);

        const auto configuration = easylocal::config::root(
            easylocal::config::named<"application">(app_parameters),
            easylocal::config::named<"search">(search_parameters));
        print_configuration(configuration);

        const auto instance = load_instance(app_parameters.instance_file);
        const AssignmentSolution initial_solution{
            .assignment = {0, 0, 1},
        };

        auto runner =
            Runner{FirstImprovement{search_parameters}}
            | (solution_manager<AssignmentSolutionManager>()
               | component<CapacityCostComponent>())
            | (neighborhood<ReassignJobNeighborhoodExplorer>()
               | delta<
                     CapacityCostComponent,
                     ReassignCapacityDeltaEvaluator>());

        const auto result = runner.bind(instance).run(initial_solution);

        std::cout << "instance:         " << app_parameters.instance_file << '\n';
        std::cout << "initial solution: ";
        print_solution(initial_solution);
        std::cout << '\n';

        std::cout << "final solution:   ";
        print_solution(result.solution);
        std::cout << '\n';

        std::cout << "final cost: overload=" << result.cost.get<0>()
                  << ", overloaded_machines=" << result.cost.get<1>() << '\n';
        std::cout << "evaluations: " << result.evaluations << '\n';
        std::cout << "termination: "
                  << (result.termination == FirstImprovementTermination::local_optimum
                          ? "local optimum"
                          : "evaluation budget exhausted")
                  << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
