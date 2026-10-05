// The tutorial's TSP as an HTTP service (REST component).
#include "tsp.hpp"

#include <easylocal/adapters/rest.hpp>
#include <easylocal/easylocal.hpp>

#include <crow.h>

#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

// [codec] ------------------------------------------------------------------
// Translates between JSON and the problem values.
struct TspCodec
{
    // {"distance": [[0, 2, 9, 10, 7], [2, 0, 6, 4, 3], ...]}, one array per row
    tutorial::Tsp decode_input(const crow::json::rvalue& payload) const
    {
        if (payload.t() != crow::json::type::Object || !payload.has("distance"))
            throw std::invalid_argument{"input must have 'distance'"};
        const auto& rows = payload["distance"];
        if (rows.t() != crow::json::type::List)
            throw std::invalid_argument{"'distance' must be a list of rows"};
        tutorial::Tsp tsp;
        for (const auto& row : rows)
        {
            if (row.t() != crow::json::type::List)
                throw std::invalid_argument{"'distance' must be a list of rows"};
            auto& values = tsp.distance.emplace_back();
            for (const auto& value : row)
            {
                if (value.t() != crow::json::type::Number)
                    throw std::invalid_argument{"'distance' must hold numbers"};
                values.push_back(value.d());
            }
        }
        for (const auto& values : tsp.distance)
            if (values.size() != tsp.cities())
                throw std::invalid_argument{"'distance' must be a square matrix"};
        return tsp;
    }

    crow::json::wvalue encode_solution(
        const tutorial::Tsp&,
        const tutorial::Tour& tour) const
    {
        crow::json::wvalue json;
        json["order"] = tour.order;
        return json;
    }

    crow::json::wvalue encode_cost(double length) const
    {
        crow::json::wvalue json;
        json["length"] = length;
        return json;
    }
};
// [codec] ------------------------------------------------------------------

} // namespace

int main(int argc, char* argv[])
{
    using namespace tutorial;
    namespace el = easylocal;
    namespace runners = easylocal::runners;
    using Classic = runners::temperature::Classic;

    const auto port =
        argc > 1 ? static_cast<std::uint16_t>(std::stoul(argv[1])) : std::uint16_t{18080};

    // [rest] ---------------------------------------------------------------
    auto application = el::app("tsp")
        | (el::solution_manager<TourManager>() | el::component<TourLength>())
        | (el::neighborhood<TwoOptExplorer>()
            | el::delta<TourLength, TwoOptLengthDelta>())
        | el::runner<runners::FirstImprovement>("fi")
        | el::runner<runners::SimulatedAnnealing<Classic>>("sa");

    // [text] -------------------------------------------------------------
    // The Input and the tours in the text of the I/O hooks (chapter 5).
    auto text_api = el::rest::blueprint("/tsp-text", application);
    // [text] -------------------------------------------------------------

    // The same app, in the JSON of TspCodec.
    auto api = el::rest::blueprint(
        "/tsp",
        std::move(application),
        TspCodec{},
        el::rest::blueprint_options{
            .workers = 2,         // background search threads
            .queue_capacity = 16, // waiting runs
            .seed = 2026,         // runs without an explicit "seed"
        });

    crow::SimpleApp server;
    server.register_blueprint(text_api.crow_blueprint());
    server.register_blueprint(api.crow_blueprint());
    // Listening on this machine only; a port that cannot be bound is an error.
    auto serving = server.bindaddr("127.0.0.1").port(port).multithreaded().run_async();
    server.wait_for_server_start();
    if (serving.wait_for(std::chrono::milliseconds{200}) == std::future_status::ready)
    {
        std::cerr << "error: cannot listen on 127.0.0.1:" << port << '\n';
        return 1;
    }
    serving.get(); // until a signal stops the server
    // [rest] ---------------------------------------------------------------
}
