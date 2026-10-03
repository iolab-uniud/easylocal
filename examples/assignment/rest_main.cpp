#include "application.hpp"
#include "instance_io.hpp"

#include <easylocal/adapters/rest.hpp>

#include <crow.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using namespace assignment;

std::vector<quantity_type> read_quantities(
    const crow::json::rvalue& payload,
    const char* key)
{
    if (!payload.has(key))
        throw std::invalid_argument{std::string{"missing JSON field '"} + key + "'"};

    const auto& array = payload[key];
    if (array.t() != crow::json::type::List)
    {
        throw std::invalid_argument{
            std::string{"JSON field '"} + key + "' must be an array"};
    }

    std::vector<quantity_type> values;
    values.reserve(array.size());
    try
    {
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
    }
    catch (const std::invalid_argument&)
    {
        throw;
    }
    catch (const std::exception&)
    {
        throw std::invalid_argument{
            std::string{"JSON field '"} + key + "' must contain integer values"};
    }
    return values;
}

struct AssignmentCodec
{
    AssignmentInstance decode_input(const crow::json::rvalue& payload) const
    {
        if (payload.t() == crow::json::type::String)
        {
            std::istringstream input{std::string{payload.s()}};
            try
            {
                return read_assignment_instance(input);
            }
            catch (const std::runtime_error& error)
            {
                throw std::invalid_argument{
                    "invalid textual assignment input: " + std::string{error.what()}};
            }
        }

        if (payload.t() != crow::json::type::Object)
        {
            throw std::invalid_argument{
                "assignment input must be a JSON object or a textual instance string"};
        }

        auto input = AssignmentInstance{
            .demand = read_quantities(payload, "demand"),
            .capacity = read_quantities(payload, "capacity"),
        };

        if (!input.demand.empty() && input.capacity.empty())
            throw std::invalid_argument{"assignment input has jobs but no machines"};
        return input;
    }

    crow::json::wvalue encode_solution(
        const AssignmentInstance&,
        const AssignmentSolution& solution) const
    {
        crow::json::wvalue json;
        json["assignment"] = solution.assignment;
        return json;
    }

    crow::json::wvalue encode_cost(const Cost& cost) const
    {
        crow::json::wvalue json;
        json["hard"]["total_overload"] = cost.hard().get<0>();
        json["hard"]["overloaded_machines"] = cost.hard().get<1>();
        json["soft"]["load_imbalance"] = cost.soft();
        return json;
    }
};

struct ServerOptions
{
    std::uint16_t port{18080};
    std::size_t completed_run_capacity{64};
};

std::size_t parse_positive_size(
    const char* value,
    std::string_view label,
    std::size_t maximum = static_cast<std::size_t>(-1))
{
    std::size_t consumed = 0;
    const auto parsed = std::stoull(value, &consumed);
    if (consumed != std::string_view{value}.size() || parsed == 0 || parsed > maximum)
    {
        throw std::invalid_argument{
            std::string{label} + " must be a positive integer in range"};
    }
    return static_cast<std::size_t>(parsed);
}

ServerOptions parse_server_options(int argc, char** argv)
{
    if (argc < 1 || argc > 3)
    {
        throw std::invalid_argument{
            "usage: easylocal_assignment_rest_mwe [port [completed-run-capacity]]"};
    }

    ServerOptions options;
    if (argc >= 2)
    {
        options.port =
            static_cast<std::uint16_t>(parse_positive_size(argv[1], "REST port", 65535));
    }
    if (argc == 3)
    {
        options.completed_run_capacity =
            parse_positive_size(argv[2], "completed-run-capacity");
    }
    return options;
}

} // namespace

int main(int argc, char** argv)
{
    ServerOptions options;
    try
    {
        options = parse_server_options(argc, argv);
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }

    auto api = easylocal::rest::blueprint(
        "/assignment",
        make_application("assignment"),
        AssignmentCodec{},
        easylocal::rest::blueprint_options{
            .workers = 2,
            .queue_capacity = 16,
            .completed_run_capacity = options.completed_run_capacity,
        });

    crow::SimpleApp server;
    server.register_blueprint(api.crow_blueprint());

    server.port(options.port).multithreaded().run();
}
