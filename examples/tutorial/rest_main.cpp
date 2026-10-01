// The tutorial's TSP as an HTTP service (REST component).
#include "tsp.hpp"

#include <easylocal/adapters/rest.hpp>
#include <easylocal/easylocal.hpp>

#include <crow.h>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace
{

// [codec] ------------------------------------------------------------------
// Translates between JSON and the problem values.
struct TspCodec
{
    // {"cities": 5, "distance": [0, 2, 9, ...]}
    [[nodiscard]] auto decode_input(const crow::json::rvalue& payload) const
        -> tutorial::Tsp
    {
        if (payload.t() != crow::json::type::Object ||
            !payload.has("cities") || !payload.has("distance"))
        {
            throw std::invalid_argument{"input must have 'cities' and 'distance'"};
        }
        tutorial::Tsp tsp{.cities = static_cast<std::size_t>(payload["cities"].u())};
        for (const auto& value : payload["distance"])
        {
            tsp.distance.push_back(value.d());
        }
        if (tsp.distance.size() != tsp.cities * tsp.cities)
        {
            throw std::invalid_argument{"'distance' must have cities * cities entries"};
        }
        return tsp;
    }

    [[nodiscard]] auto encode_solution(
        const tutorial::Tsp&,
        const tutorial::Tour& tour) const -> crow::json::wvalue
    {
        crow::json::wvalue json;
        json["order"] = tour.order;
        return json;
    }

    [[nodiscard]] auto encode_cost(const double length) const -> crow::json::wvalue
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

    const auto port = argc > 1 ? static_cast<std::uint16_t>(std::stoul(argv[1]))
                               : std::uint16_t{18080};

    // [rest] ---------------------------------------------------------------
    auto application = el::app("tsp")
        | (el::solution_manager<TourManager>() | el::component<TourLength>())
        | (el::neighborhood<TwoOptExplorer>() | el::delta<TourLength, TwoOptLengthDelta>())
        | el::runner<runners::FirstImprovement>("fi")
        | el::runner<runners::SimulatedAnnealing<Classic>>("sa");

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
    server.register_blueprint(api.crow_blueprint());
    server.port(port).multithreaded().run();
    // [rest] ---------------------------------------------------------------
}
