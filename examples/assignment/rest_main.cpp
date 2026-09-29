#include "capacity_delta.hpp"
#include "cost_components.hpp"
#include "demo_runner.hpp"
#include "instance.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <easylocal/app.hpp>
#include <easylocal/rest.hpp>
#include <easylocal/search/first_improvement.hpp>

#include <crow.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
        .runner<easylocal::runner::first_improvement>("fi")
        .runner<demo::slow_first_improvement>("slow-fi");

    application
        .runner_config<easylocal::runner::first_improvement>()
        .max_evaluations = 100;

    auto& slow_config =
        application.runner_config<demo::slow_first_improvement>();
    slow_config.max_evaluations = 2000;
    slow_config.delay_ms = 5;
    return application;
}

[[nodiscard]] auto read_quantities(
    const crow::json::rvalue& payload,
    const char* key) -> std::vector<quantity_type>
{
    if (!payload.has(key))
    {
        throw std::invalid_argument{
            std::string{"missing JSON field '"} + key + "'"};
    }

    std::vector<quantity_type> values;
    const auto& array = payload[key];
    values.reserve(array.size());
    for (const auto& value : array)
    {
        const auto decoded = value.i();
        if (decoded < 0)
        {
            throw std::invalid_argument{
                std::string{"JSON field '"} + key + "' contains a negative value"};
        }
        values.push_back(static_cast<quantity_type>(decoded));
    }
    return values;
}

struct AssignmentCodec
{
    [[nodiscard]] auto decode_input(const crow::json::rvalue& payload) const
        -> AssignmentInstance
    {
        auto input = AssignmentInstance{
            .demand = read_quantities(payload, "demand"),
            .capacity = read_quantities(payload, "capacity"),
        };

        if (!input.demand.empty() && input.capacity.empty())
        {
            throw std::invalid_argument{
                "assignment input has jobs but no machines"};
        }
        return input;
    }

    [[nodiscard]] auto encode_solution(
        const AssignmentInstance&,
        const AssignmentSolution& solution) const -> crow::json::wvalue
    {
        crow::json::wvalue json;
        json["assignment"] = solution.assignment;
        return json;
    }

    [[nodiscard]] auto encode_cost(const Cost& cost) const
        -> crow::json::wvalue
    {
        crow::json::wvalue json;
        json["hard"]["total_overload"] = cost.hard().get<0>();
        json["hard"]["overloaded_machines"] = cost.hard().get<1>();
        json["soft"]["load_imbalance"] = cost.soft();
        return json;
    }
};

} // namespace

[[nodiscard]] auto parse_port(const int argc, char** argv) -> std::uint16_t
{
    if (argc == 1)
    {
        return 18080;
    }
    if (argc != 2)
    {
        throw std::invalid_argument{"usage: easylocal_assignment_rest_mwe [port]"};
    }

    std::size_t consumed = 0;
    const auto parsed = std::stoul(argv[1], &consumed);
    if (consumed != std::string_view{argv[1]}.size() || parsed == 0 ||
        parsed > 65535)
    {
        throw std::invalid_argument{"REST port must be an integer in 1..65535"};
    }
    return static_cast<std::uint16_t>(parsed);
}

int main(int argc, char** argv)
{
    const auto port = parse_port(argc, argv);

    auto api = easylocal::rest::blueprint(
        "/assignment",
        make_application(),
        AssignmentCodec{},
        easylocal::rest::blueprint_options{
            .workers = 2,
            .queue_capacity = 16,
        });

    crow::SimpleApp server;
    server.register_blueprint(api.crow_blueprint());

    server.port(port)
        .multithreaded()
        .run();
}
