// The REST blueprint answering requests in-process (no network): unknown runs,
// runs that fail, a codec that fails, a run from a given initial solution, a
// run with a target cost, a full queue and a run cancelled while still queued.
#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/instance.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"
#include "support/assignment_capacity_delta.hpp"

#include <easylocal/adapters/rest.hpp>
#include <easylocal/app/app.hpp>
#include <easylocal/runners/first_improvement.hpp>

#include <crow.h>

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
using namespace assignment;
using namespace std::chrono_literals;

// Held closed by the test while it fills the queue behind a running run.
class gate
{
public:
    void wait()
    {
        std::unique_lock lock{mutex_};
        opened_changed_.wait(lock, [this] { return opened_; });
    }

    void open()
    {
        {
            const std::lock_guard lock{mutex_};
            opened_ = true;
        }
        opened_changed_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable opened_changed_;
    bool opened_{false};
};

gate run_gate;

[[nodiscard]] auto first_improvement()
{
    return easylocal::runners::FirstImprovement{
        easylocal::runners::FirstImprovementParameters{.max_evaluations = 100}};
}

struct NoParameters
{
};

// First Improvement once the gate is open.
class GatedRunner
{
public:
    using parameters_type = NoParameters;

    explicit GatedRunner(NoParameters) {}

    template<class Run>
    auto run(Run& run, Run::solution_type solution) const
    {
        run_gate.wait();
        return first_improvement().run(run, std::move(solution));
    }
};

// Fails as soon as it starts, with a std::exception or with something else.
template<bool StandardException>
class BrokenRunner
{
public:
    using parameters_type = NoParameters;

    explicit BrokenRunner(NoParameters) {}

    template<class Run>
    auto run(Run& run, Run::solution_type solution) const
    {
        // Always taken; the return type is still First Improvement's.
        if (!solution.assignment.empty())
        {
            if constexpr (StandardException)
                throw std::runtime_error{"the search broke"};
            else
                throw 42;
        }
        return first_improvement().run(run, std::move(solution));
    }
};

[[nodiscard]] auto make_application()
{
    auto sm = easylocal::solution_manager<AssignmentSolutionManager>()
        | assignment::assignment_cost();

    auto nhe = easylocal::neighborhood<ReassignJobNeighborhoodExplorer>()
        | easylocal::delta<CapacityCostComponent, ReassignCapacityDeltaEvaluator>();

    auto application =
        easylocal::app("assignment")
            .with_solution_manager(std::move(sm))
            .with_neighborhood(std::move(nhe))
            .with_runner<easylocal::runners::FirstImprovement>("fi")
            .with_runner<GatedRunner>("gated")
            .with_runner<BrokenRunner<true>>("broken")
            .with_runner<BrokenRunner<false>>("very-broken");
    application.runner_config<easylocal::runners::FirstImprovement>().max_evaluations =
        100;
    return application;
}

using app_type = decltype(make_application());
using runtime_type = decltype(std::declval<const app_type&>().bind(
    std::declval<const AssignmentInstance&>()));
using solution_type = typename runtime_type::solution_manager_type::solution_type;
using cost_type = typename runtime_type::solution_manager_type::cost_type;

struct AssignmentCodec
{
    // {"corrupt": ...} stands for a codec bug: an exception other than
    // std::invalid_argument, which would be a bad request.
    [[nodiscard]] auto decode_input(const crow::json::rvalue& payload) const
        -> AssignmentInstance
    {
        if (payload.has("corrupt"))
        {
            if (payload["corrupt"].t() == crow::json::type::String)
                throw std::runtime_error{"corrupt input"};
            throw 42;
        }
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

    // {"hard": [overload, overloaded machines], "soft": imbalance}.
    [[nodiscard]] auto decode_cost(const crow::json::rvalue& payload) const -> cost_type
    {
        if (payload.t() != crow::json::type::Object || !payload.has("hard")
            || !payload.has("soft") || payload["hard"].t() != crow::json::type::List
            || payload["hard"].size() != 2)
        {
            throw std::invalid_argument{
                "'target' must be {\"hard\": [a, b], \"soft\": c}"};
        }
        using hard_type = typename cost_type::hard_cost_type;
        using soft_type = typename cost_type::soft_cost_type;
        return cost_type{
            hard_type(payload["hard"][0].i(), payload["hard"][1].i()),
            static_cast<soft_type>(payload["soft"].d())};
    }

    [[nodiscard]] auto encode_solution(
        const AssignmentInstance&,
        const solution_type&) const -> crow::json::wvalue
    {
        return crow::json::wvalue::empty_object();
    }

    [[nodiscard]] auto encode_cost(const cost_type&) const -> crow::json::wvalue
    {
        return crow::json::wvalue::empty_object();
    }
};

struct reply
{
    int code{};
    crow::json::rvalue body;
};

[[nodiscard]] auto send(
    crow::SimpleApp& server,
    const crow::HTTPMethod method,
    const std::string& url,
    std::string body = {}) -> reply
{
    crow::request request;
    request.method = method;
    request.url = url;
    request.raw_url = url;
    request.body = std::move(body);
    crow::response response;
    server.handle_full(request, response);
    return {.code = response.code, .body = crow::json::load(response.body)};
}

[[nodiscard]] auto text(const crow::json::rvalue& value) -> std::string
{
    return std::string{value.s()};
}

[[nodiscard]] auto submit(
    crow::SimpleApp& server,
    const std::string& runner,
    std::string body) -> reply
{
    return send(
        server,
        crow::HTTPMethod::POST,
        "/assignment/runners/" + runner + "/runs",
        std::move(body));
}

// The run's status once it equals `status`; fails the test after 10 s.
auto wait_for(crow::SimpleApp& server, const std::string& id, const std::string& status)
    -> crow::json::rvalue
{
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (true)
    {
        auto current = send(server, crow::HTTPMethod::GET, "/assignment/runs/" + id);
        assert(current.code == 200);
        if (text(current.body["status"]) == status)
            return std::move(current.body); // a copy of an rvalue loses its keys
        assert(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(5ms);
    }
}

void unknown_runs_are_not_found(crow::SimpleApp& server)
{
    for (const auto& [method, url] :
        std::vector<std::pair<crow::HTTPMethod, std::string>>{
            {crow::HTTPMethod::GET, "/assignment/runs/42"},
            {crow::HTTPMethod::GET, "/assignment/runs/42/solution"},
            {crow::HTTPMethod::POST, "/assignment/runs/42/cancel"},
            {crow::HTTPMethod::DELETE, "/assignment/runs/42"},
        })
    {
        const auto answer = send(server, method, url);
        assert(answer.code == 404);
        assert(text(answer.body["error"]["code"]) == "run_not_found");
        assert(text(answer.body["error"]["message"]) == "run '42' does not exist");
    }
}

void a_failing_run_reports_its_error(crow::SimpleApp& server)
{
    const auto submitted = submit(server, "broken", R"({"input": {}})");
    assert(submitted.code == 202);
    const auto id = text(submitted.body["id"]);

    const auto status = wait_for(server, id, "failed");
    assert(text(status["error"]["code"]) == "run_failed");
    assert(text(status["error"]["message"]) == "the search broke");
    assert(!status.has("solution_url"));

    const auto solution =
        send(server, crow::HTTPMethod::GET, "/assignment/runs/" + id + "/solution");
    assert(solution.code == 409);
    assert(text(solution.body["error"]["code"]) == "run_failed");
    assert(text(solution.body["error"]["message"]) == "the search broke");
}

void a_run_failing_without_a_standard_exception_reports_an_unknown_error(
    crow::SimpleApp& server)
{
    const auto submitted = submit(server, "very-broken", R"({"input": {}})");
    assert(submitted.code == 202);
    const auto status = wait_for(server, text(submitted.body["id"]), "failed");
    assert(text(status["error"]["message"]) == "unknown runner error");
}

void a_codec_failure_is_an_internal_error(crow::SimpleApp& server)
{
    const auto standard = submit(server, "fi", R"({"input": {"corrupt": "yes"}})");
    assert(standard.code == 500);
    assert(text(standard.body["error"]["code"]) == "internal_error");
    assert(text(standard.body["error"]["message"]) == "cannot create run: corrupt input");

    const auto other = submit(server, "fi", R"({"input": {"corrupt": 1}})");
    assert(other.code == 500);
    assert(text(other.body["error"]["message"]) == "cannot create run: unknown error");
}

void a_run_starts_from_the_given_initial_solution(crow::SimpleApp& server)
{
    const auto submitted =
        submit(server, "fi", R"({"input": {}, "initial_solution": {}})");
    assert(submitted.code == 202);
    const auto id = text(submitted.body["id"]);

    const auto status = wait_for(server, id, "succeeded");
    assert(text(status["solution_url"]) == "/assignment/runs/" + id + "/solution");
    assert(send(server, crow::HTTPMethod::GET, text(status["solution_url"])).code == 200);
}

void a_run_stops_at_its_target(crow::SimpleApp& server)
{
    // Any cost is at least as good as this target: the run stops at the
    // initial evaluation.
    const auto submitted = submit(
        server,
        "fi",
        R"({"input": {}, "target": {"hard": [100, 100], "soft": 1000}})");
    assert(submitted.code == 202);
    assert(submitted.body.has("target"));
    const auto reached = wait_for(server, text(submitted.body["id"]), "succeeded");
    assert(reached.has("target"));
    assert(reached["progress"]["evaluations"].u() == 1);

    const auto untargeted = submit(server, "fi", R"({"input": {}})");
    assert(!untargeted.body.has("target"));
    const auto complete = wait_for(server, text(untargeted.body["id"]), "succeeded");
    assert(complete["progress"]["evaluations"].u() > 1);

    const auto invalid = submit(server, "fi", R"({"input": {}, "target": 3})");
    assert(invalid.code == 422);
    assert(text(invalid.body["error"]["code"]) == "invalid_run_request");
}

void an_arithmetic_target_is_a_number()
{
    using easylocal::rest::detail::decode_arithmetic_cost;
    assert(decode_arithmetic_cost<int>(crow::json::load("3")) == 3);
    assert(decode_arithmetic_cost<int>(crow::json::load("-2")) == -2);
    assert(decode_arithmetic_cost<double>(crow::json::load("2.5")) == 2.5);

    const auto rejects = [](const char* json, auto cost) {
        try
        {
            [[maybe_unused]] const auto decoded =
                decode_arithmetic_cost<decltype(cost)>(crow::json::load(json));
        }
        catch (const std::invalid_argument&)
        {
            return true;
        }
        return false;
    };
    assert(rejects("2.5", 0));
    assert(rejects(R"("3")", 0.0));
}

void a_full_queue_rejects_runs_and_a_queued_run_can_be_cancelled(crow::SimpleApp& server)
{
    // One worker, a queue of one: the first run holds the worker at the gate.
    const auto running = text(submit(server, "gated", R"({"input": {}})").body["id"]);
    wait_for(server, running, "running");

    const auto queued = submit(server, "gated", R"({"input": {}})");
    assert(queued.code == 202);
    const auto queued_id = text(queued.body["id"]);
    wait_for(server, queued_id, "queued");

    const auto rejected = submit(server, "gated", R"({"input": {}})");
    assert(rejected.code == 503);
    assert(text(rejected.body["error"]["code"]) == "queue_full");

    const auto cancelled =
        send(server, crow::HTTPMethod::POST, "/assignment/runs/" + queued_id + "/cancel");
    assert(cancelled.code == 202);
    assert(cancelled.body["cancellation_requested"].b());

    run_gate.open();
    wait_for(server, running, "succeeded");
    const auto never_started = wait_for(server, queued_id, "cancelled");
    assert(never_started["progress"]["evaluations"].u() == 0);

    const auto solution = send(
        server,
        crow::HTTPMethod::GET,
        "/assignment/runs/" + queued_id + "/solution");
    assert(solution.code == 409);
    assert(text(solution.body["error"]["code"]) == "result_not_ready");

    assert(
        send(server, crow::HTTPMethod::DELETE, "/assignment/runs/" + queued_id).code
        == 204);
    assert(
        send(server, crow::HTTPMethod::GET, "/assignment/runs/" + queued_id).code == 404);
}

void an_empty_prefix_is_rejected()
{
    bool rejected = false;
    try
    {
        [[maybe_unused]] auto api =
            easylocal::rest::blueprint("/", make_application(), AssignmentCodec{});
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    assert(rejected);
}

} // namespace

int main()
{
    auto api = easylocal::rest::blueprint(
        "/assignment/",
        make_application(),
        AssignmentCodec{},
        easylocal::rest::blueprint_options{
            .workers = 1,
            .queue_capacity = 1,
            .completed_run_capacity = 8,
        });
    assert(api.prefix() == "assignment");

    crow::SimpleApp server;
    server.loglevel(crow::LogLevel::Warning);
    server.register_blueprint(api.crow_blueprint());
    server.add_blueprint(); // what run() does before serving
    server.validate();

    unknown_runs_are_not_found(server);
    a_failing_run_reports_its_error(server);
    a_run_failing_without_a_standard_exception_reports_an_unknown_error(server);
    a_codec_failure_is_an_internal_error(server);
    a_run_starts_from_the_given_initial_solution(server);
    a_run_stops_at_its_target(server);
    an_arithmetic_target_is_a_number();
    a_full_queue_rejects_runs_and_a_queued_run_can_be_cancelled(server);
    an_empty_prefix_is_rejected();
    run_gate.open(); // never leave a worker waiting on exit
}
