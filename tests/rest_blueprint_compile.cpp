#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/instance.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"
#include "support/assignment_capacity_delta.hpp"

#include <easylocal/adapters/rest.hpp>
#include <easylocal/app/app.hpp>
#include <easylocal/runners/first_improvement.hpp>

#include <crow.h>

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
using namespace assignment;

[[nodiscard]] auto make_application()
{
    auto sm = easylocal::solution_manager<AssignmentSolutionManager>()
        | assignment::assignment_cost();

    auto nhe =
        easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
        | easylocal::delta<
              CapacityCostComponent,
              ReassignCapacityDeltaEvaluator>();

    auto application = easylocal::app("assignment")
        .with_solution_manager(std::move(sm))
        .with_neighborhood(std::move(nhe))
        .with_runner<easylocal::runners::FirstImprovement>("fi");

    application
        .runner_config<easylocal::runners::FirstImprovement>()
        .max_evaluations = 100;
    return application;
}

using app_type = decltype(make_application());
using runtime_type = decltype(
    std::declval<const app_type&>().for_input(
        std::declval<const AssignmentInstance&>()));
using solution_type = typename runtime_type::solution_manager_type::solution_type;
using cost_type = typename runtime_type::solution_manager_type::cost_type;

struct AssignmentCodec
{
    [[nodiscard]] auto decode_input(const crow::json::rvalue&) const
        -> AssignmentInstance
    {
        return AssignmentInstance{
            .demand = {4, 4, 2},
            .capacity = {5, 5},
        };
    }

    [[nodiscard]] auto decode_initial_solution(
        const AssignmentInstance& input,
        const crow::json::rvalue&) const -> solution_type
    {
        return solution_type{
            .assignment = std::vector<machine_id>(input.demand.size(), 0),
        };
    }

    [[nodiscard]] auto encode_solution(
        const AssignmentInstance&,
        const solution_type&) const -> crow::json::wvalue
    {
        return crow::json::wvalue::empty_object();
    }

    [[nodiscard]] auto encode_cost(const cost_type&) const
        -> crow::json::wvalue
    {
        return crow::json::wvalue::empty_object();
    }
};

} // namespace

int main()
{
    auto api = easylocal::rest::blueprint(
        "/assignment",
        make_application(),
        AssignmentCodec{},
        easylocal::rest::blueprint_options{
            .workers = 1,
            .queue_capacity = 1,
            .completed_run_capacity = 2,
        });

    crow::SimpleApp server;
    server.register_blueprint(api.crow_blueprint());

    bool zero_retention_rejected = false;
    try
    {
        [[maybe_unused]] auto invalid = easylocal::rest::blueprint(
            "/invalid",
            make_application(),
            AssignmentCodec{},
            easylocal::rest::blueprint_options{
                .workers = 1,
                .queue_capacity = 1,
                .completed_run_capacity = 0,
            });
    }
    catch (const std::invalid_argument&)
    {
        zero_retention_rejected = true;
    }

    return api.prefix() == "assignment" && zero_retention_rejected ? 0 : 1;
}
