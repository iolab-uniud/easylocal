#include "../examples/assignment/capacity_delta.hpp"
#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/instance.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"

#include <easylocal/app.hpp>
#include <easylocal/rest.hpp>
#include <easylocal/search/first_improvement.hpp>

#include <crow.h>

#include <cstddef>
#include <optional>
#include <utility>

namespace
{
using namespace easylocal::mwe::assignment;

[[nodiscard]] auto make_application()
{
    auto sm =
        easylocal::solution_manager<AssignmentSolutionManager>()
        | easylocal::component<CapacityCostComponent>()
        | easylocal::component<LoadImbalanceCostComponent>()
        | easylocal::aggregator(AssignmentCostAggregator{});

    auto nhe =
        easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
        | easylocal::delta<
              CapacityCostComponent,
              ReassignCapacityDeltaEvaluator>();

    auto application = easylocal::app("assignment")
        .solution_manager(std::move(sm))
        .neighborhood(std::move(nhe))
        .runner<easylocal::runner::first_improvement>("fi");

    application
        .runner_config<easylocal::runner::first_improvement>()
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
        const AssignmentInstance&,
        const crow::json::rvalue&) const -> std::optional<solution_type>
    {
        return std::nullopt;
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
        });

    crow::SimpleApp server;
    server.register_blueprint(api.crow_blueprint());
    return api.prefix() == "assignment" ? 0 : 1;
}
