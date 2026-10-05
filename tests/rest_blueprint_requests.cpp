// The REST blueprint answering requests in-process (no network): unknown runs,
// runs that fail, a codec that fails, a run from a given initial solution, a
// run with a target cost, runs with their own parameters, a deeply nested body,
// a full queue, a run cancelled while still queued, and a problem served
// through its text hooks, without a codec.
#include "../examples/assignment/cost.hpp"
#include "../examples/assignment/cost_components.hpp"
#include "../examples/assignment/instance.hpp"
#include "../examples/assignment/neighborhood_explorer.hpp"
#include "../examples/assignment/solution_manager.hpp"
#include "../examples/tutorial/tsp.hpp"
#include "support/assignment_capacity_delta.hpp"

#include <easylocal/adapters/rest.hpp>
#include <easylocal/app/app.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/solvers/pipeline.hpp>

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

// Runs until it is asked to stop.
class EndlessRunner
{
public:
    using parameters_type = NoParameters;

    explicit EndlessRunner(NoParameters) {}

    template<class Run>
    auto run(Run& run, Run::solution_type solution) const
    {
        auto current = run.start(solution);
        while (!run.should_stop())
            std::this_thread::sleep_for(1ms);
        return run.finish(std::move(solution), current.cost());
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

    // A pipeline of two descents, run by name like a runner.
    auto descent =
        easylocal::make_runner<easylocal::runners::FirstImprovement>(
            {.max_evaluations = 100})
        | sm | nhe;

    auto application =
        easylocal::app("assignment")
            .with_solution_manager(sm)
            .with_neighborhood(nhe)
            .with_runner<easylocal::runners::FirstImprovement>("fi")
            .with_runner<GatedRunner>("gated")
            .with_runner<EndlessRunner>("endless")
            .with_runner<BrokenRunner<true>>("broken")
            .with_runner<BrokenRunner<false>>("very-broken")
            .with_pipeline(
                easylocal::pipeline(
                    "cascade",
                    easylocal::solvers::stage("first", descent),
                    easylocal::solvers::stage("second", descent)));
    application.runner_parameters<easylocal::runners::FirstImprovement>("fi")
        .max_evaluations = 100;
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

    // {"machine": m} assigns every job to machine m, which may not exist.
    [[nodiscard]] auto decode_initial_solution(
        const AssignmentInstance& input,
        const crow::json::rvalue& payload) const -> solution_type
    {
        const auto machine = payload.has("machine")
            ? static_cast<machine_id>(payload["machine"].u())
            : machine_id{0};
        return solution_type{
            .assignment = std::vector<machine_id>(input.demand.size(), machine),
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

    // A soft cost of 666 stands for a cost the codec cannot encode.
    [[nodiscard]] auto encode_cost(const cost_type& cost) const -> crow::json::wvalue
    {
        if (cost.soft() == 666)
            throw std::runtime_error{"cannot encode the cost"};
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

// The status of the run at url once it equals `status`; fails the test after
// 10 s.
auto wait_for_run(
    crow::SimpleApp& server,
    const std::string& url,
    const std::string& status) -> crow::json::rvalue
{
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (true)
    {
        auto current = send(server, crow::HTTPMethod::GET, url);
        assert(current.code == 200);
        if (text(current.body["status"]) == status)
            return std::move(current.body); // a copy of an rvalue loses its keys
        assert(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(5ms);
    }
}

// The status of the assignment run `id` once it equals `status`.
auto wait_for(crow::SimpleApp& server, const std::string& id, const std::string& status)
    -> crow::json::rvalue
{
    return wait_for_run(server, "/assignment/runs/" + id, status);
}

void unknown_runs_are_not_found(crow::SimpleApp& server)
{
    for (const auto& [method, url] :
        std::vector<std::pair<crow::HTTPMethod, std::string>>{
            {crow::HTTPMethod::GET, "/assignment/runs/42"},
            {crow::HTTPMethod::GET, "/assignment/runs/42/solution"},
            {crow::HTTPMethod::POST, "/assignment/runs/42/cancel"},
            {crow::HTTPMethod::Delete, "/assignment/runs/42"},
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

void a_pipeline_runs_by_name(crow::SimpleApp& server)
{
    // Listed among the runners, its stages' parameters under its name.
    const auto runners = send(server, crow::HTTPMethod::GET, "/assignment/runners");
    bool listed = false;
    for (const auto& name : runners.body["runners"])
        listed = listed || text(name) == "cascade";
    assert(listed);
    const auto parameters = send(server, crow::HTTPMethod::GET, "/assignment/parameters");
    bool configurable = false;
    for (const auto& parameter : parameters.body["parameters"])
        configurable =
            configurable || text(parameter["path"]) == "runners.cascade.second.attempts";
    assert(configurable);

    // Run by name, with a parameter of its own for this run.
    const auto submitted = submit(
        server,
        "cascade",
        R"({"input": {}, "parameters": {"runners.cascade.second.attempts": 2}})");
    assert(submitted.code == 202);
    const auto done = wait_for(server, text(submitted.body["id"]), "succeeded");
    assert(text(done["parameters"]["runners.cascade.second.attempts"]) == "2");
}

void a_deeply_nested_body_is_rejected_before_parsing(crow::SimpleApp& server)
{
    // Deep enough to exhaust the stack of a Crow thread while being parsed.
    const std::string deep(10000, '[');
    const auto rejected = submit(server, "fi", deep + std::string(10000, ']'));
    assert(rejected.code == 400);
    assert(text(rejected.body["error"]["code"]) == "invalid_json");

    // Brackets inside strings do not nest.
    const auto quoted = submit(
        server,
        "fi",
        R"({"input": {"name": ")" + deep + R"(\"["}, "timeout": 0})");
    assert(quoted.code == 202);
    wait_for(server, text(quoted.body["id"]), "succeeded");

    using easylocal::rest::detail::json_nests_deeper_than;
    assert(!json_nests_deeper_than("[[]]", 2));
    assert(json_nests_deeper_than("[[[]]]", 2));
    assert(!json_nests_deeper_than(R"(["\"[[[", {}])", 2));
}

void a_run_has_a_time_limit(crow::SimpleApp& server)
{
    // No time left: the run ends at once, and its status keeps the limit.
    const auto submitted = submit(server, "fi", R"({"input": {}, "timeout": 0})");
    assert(submitted.code == 202);
    const auto done = wait_for(server, text(submitted.body["id"]), "succeeded");
    assert(done["timeout"].d() == 0.0);

    for (const auto* const body :
        {R"({"input": {}, "timeout": -1})", R"({"input": {}, "timeout": "soon"})"})
    {
        const auto rejected = submit(server, "fi", body);
        assert(rejected.code == 422);
        assert(text(rejected.body["error"]["message"]).starts_with("'timeout' must be"));
    }

    // An evaluation budget: one evaluation, the initial one.
    const auto budgeted = submit(server, "fi", R"({"input": {}, "max_evaluations": 1})");
    assert(budgeted.code == 202);
    const auto spent = wait_for(server, text(budgeted.body["id"]), "succeeded");
    assert(spent["max_evaluations"].u() == 1);
    assert(spent["progress"]["evaluations"].u() == 1);
    for (const auto* const body :
        {R"({"input": {}, "max_evaluations": -1})",
            R"({"input": {}, "max_evaluations": 1.5})"})
    {
        const auto rejected = submit(server, "fi", body);
        assert(rejected.code == 422);
        assert(text(rejected.body["error"]["message"])
                .starts_with("'max_evaluations' must be"));
    }
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

    // A solution not valid for the Input is rejected before the run starts.
    const auto invalid =
        submit(server, "fi", R"({"input": {}, "initial_solution": {"machine": 7}})");
    assert(invalid.code == 422);
    assert(text(invalid.body["error"]["code"]) == "invalid_run_request");
    assert(
        text(invalid.body["error"]["message"])
        == "'initial_solution' is not a valid solution for the input");
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

    // A string is the textual syntax of costs, the command line's and the
    // TextUI's: [hard, soft], here with a lexicographic hard cost.
    const auto written =
        submit(server, "fi", R"({"input": {}, "target": "[[100, 100], 1000]"})");
    assert(written.code == 202);
    const auto written_done = wait_for(server, text(written.body["id"]), "succeeded");
    assert(written_done["progress"]["evaluations"].u() == 1);

    const auto misspelt = submit(server, "fi", R"({"input": {}, "target": "[1]"})");
    assert(misspelt.code == 422);
    assert(text(misspelt.body["error"]["message"]).starts_with("'target': "));
}

void a_run_whose_response_fails_is_not_registered(crow::SimpleApp& server)
{
    const auto runs = [&server] {
        return send(server, crow::HTTPMethod::GET, "/assignment/").body["runs"].u();
    };
    const auto before = runs();
    const auto failed =
        submit(server, "fi", R"({"input": {}, "target": {"hard": [0, 0], "soft": 666}})");
    assert(failed.code == 500);
    assert(
        text(failed.body["error"]["message"])
        == "cannot create run: cannot encode the cost");
    assert(runs() == before);
}

void a_run_has_its_own_parameters(crow::SimpleApp& server)
{
    // The app's parameters, as text, with the values runs use by default.
    const auto listed = send(server, crow::HTTPMethod::GET, "/assignment/parameters");
    assert(listed.code == 200);
    bool found = false;
    for (const auto& parameter : listed.body["parameters"])
    {
        // Every parameter can be changed by a run: none is read-only.
        assert(!parameter.has("read_only"));
        assert(parameter.has("kind") && parameter.has("active"));
        if (text(parameter["path"]) == "runners.fi.max_evaluations")
        {
            found = text(parameter["value"]) == "100";
            assert(text(parameter["kind"]) == "limit");
            assert(!text(parameter["domain"]).empty());
            assert(parameter["active"].b());
            assert(!parameter.has("condition"));
        }
    }
    assert(found);

    // Nested objects and dotted paths alike: a budget of one evaluation.
    for (const auto* const body :
        {R"({"input": {}, "parameters": {"runners": {"fi": {"max_evaluations": 1}}}})",
            R"({"input": {}, "parameters": {"runners.fi.max_evaluations": 1}})",
            R"({"input": {}, "parameters": {"runners.fi": {"max_evaluations": "1"}}})"})
    {
        const auto submitted = submit(server, "fi", body);
        assert(submitted.code == 202);
        const auto done = wait_for(server, text(submitted.body["id"]), "succeeded");
        assert(done["progress"]["evaluations"].u() == 1);
        // Reported by path, as text, for the run to be repeated.
        assert(text(done["parameters"]["runners.fi.max_evaluations"]) == "1");
    }

    // Only that run: the next one has the app's budget again.
    const auto later = submit(server, "fi", R"({"input": {}})");
    assert(!later.body.has("parameters"));
    const auto complete = wait_for(server, text(later.body["id"]), "succeeded");
    assert(complete["progress"]["evaluations"].u() > 1);

    const auto unknown = submit(
        server,
        "fi",
        R"({"input": {}, "parameters": {"runners.fi.max_evalutions": 1}})");
    assert(unknown.code == 422);
    assert(text(unknown.body["error"]["code"]) == "invalid_parameters");
    assert(text(unknown.body["error"]["message"])
            .starts_with("runners.fi.max_evalutions: "));

    const auto malformed = submit(server, "fi", R"({"input": {}, "parameters": [1, 2]})");
    assert(malformed.code == 422);
    assert(text(malformed.body["error"]["code"]) == "invalid_run_request");

    for (const auto* const body :
        {R"({"input": {}, "parameters": {"": 1}})",
            R"({"input": {}, "parameters": {"runners": {"": 1}}})"})
    {
        const auto empty = submit(server, "fi", body);
        assert(empty.code == 422);
        assert(text(empty.body["error"]["message"]).ends_with("a key is empty"));
    }
}

void json_parameter_values_become_text()
{
    using easylocal::rest::detail::parameter_text;
    assert(parameter_text(crow::json::load("0.1"), "p") == "0.1");
    assert(parameter_text(crow::json::load("-3"), "p") == "-3");
    assert(parameter_text(crow::json::load("true"), "p") == "true");
    assert(parameter_text(crow::json::load(R"("eil51.tsp")"), "p") == "eil51.tsp");
    assert(parameter_text(crow::json::load("[1, 2.5]"), "p") == "[1, 2.5]");
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
    // Out of the range of the cost type: never wrapped.
    assert(rejects("300", static_cast<signed char>(0)));
    assert(rejects("18446744073709551615", 0LL));
    assert(rejects("1e300", 0.0F));
}

// The tutorial's TSP has text hooks and no codec: the Input and the solutions
// are JSON strings in their text format, the cost a JSON number.
void a_problem_with_text_hooks_needs_no_codec()
{
    using namespace tutorial;
    auto application = easylocal::app("tsp")
        | (easylocal::solution_manager<TourManager>()
            | easylocal::component<TourLength>())
        | (easylocal::neighborhood<TwoOptExplorer>()
            | easylocal::delta<TourLength, TwoOptLengthDelta>())
        | easylocal::runner<easylocal::runners::FirstImprovement>("fi");
    auto api = easylocal::rest::blueprint(
        "/text",
        std::move(application),
        easylocal::rest::blueprint_options{.workers = 1});
    crow::SimpleApp server;
    server.loglevel(crow::LogLevel::Warning);
    server.register_blueprint(api.crow_blueprint());
    server.add_blueprint();
    server.validate();
    const auto post = [&server](std::string body) {
        return send(
            server,
            crow::HTTPMethod::POST,
            "/text/runners/fi/runs",
            std::move(body));
    };

    const std::string input =
        R"("5\n0 2 9 10 7\n2 0 6 4 3\n9 6 0 8 5\n10 4 8 0 6\n7 3 5 6 0\n")";
    const auto submitted =
        post(R"({"input": )" + input + R"(, "initial_solution": "0 1 2 3 4"})");
    assert(submitted.code == 202);
    const auto id = text(submitted.body["id"]);
    wait_for_run(server, "/text/runs/" + id, "succeeded");
    const auto solved =
        send(server, crow::HTTPMethod::GET, "/text/runs/" + id + "/solution");
    assert(solved.code == 200);
    assert(solved.body["cost"].d() == 26.0);
    assert(solved.body["solution"].t() == crow::json::type::String);
    assert(text(solved.body["solution"]).size() == 11); // five "c ", then "\n"

    // A value that is not a string, or text the hook cannot read, is a bad
    // request.
    const auto not_text = post(R"({"input": {"distance": []}})");
    assert(not_text.code == 422);
    assert(
        text(not_text.body["error"]["message"]).starts_with("'input' must be a string"));
    const auto unreadable = post(R"({"input": "five"})");
    assert(unreadable.code == 422);
    assert(text(unreadable.body["error"]["message"]) == "'input': invalid TSP header");
    const auto bad_tour =
        post(R"({"input": )" + input + R"(, "initial_solution": "0 1"})");
    assert(bad_tour.code == 422);
    assert(text(bad_tour.body["error"]["message"]).starts_with("'initial_solution': "));

    // The start: random by default for a problem with random solutions, drawn
    // from the run's seed, so the same seed starts from the same tour.
    const auto solution_of = [&](const std::string& body) {
        const auto run = post(body);
        assert(run.code == 202);
        const auto run_id = text(run.body["id"]);
        wait_for_run(server, "/text/runs/" + run_id, "succeeded");
        auto solved_run =
            send(server, crow::HTTPMethod::GET, "/text/runs/" + run_id + "/solution");
        return std::pair{text(run.body["start"]), text(solved_run.body["solution"])};
    };
    // With no evaluations the solution is the start.
    const auto budget = R"(, "max_evaluations": 1, "seed": 3})";
    const auto [default_start, first] = solution_of(R"({"input": )" + input + budget);
    assert(default_start == "random");
    const auto [random_start, again] =
        solution_of(R"({"input": )" + input + R"(, "start": "random")" + budget);
    assert(random_start == "random" && again == first);
    const auto [initial_start, initial] =
        solution_of(R"({"input": )" + input + R"(, "start": "initial")" + budget);
    assert(initial_start == "initial" && initial == "0 1 2 3 4 \n");
    const auto [given_start, given] = solution_of(
        R"({"input": )" + input + R"(, "initial_solution": "4 3 2 1 0")" + budget);
    assert(given_start == "solution" && given == "4 3 2 1 0 \n");

    const auto bad_start = post(R"({"input": )" + input + R"(, "start": "greedy"})");
    assert(bad_start.code == 422);
    const auto both = post(
        R"({"input": )" + input
        + R"(, "start": "random", "initial_solution": "0 1 2 3 4"})");
    assert(both.code == 422);
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

    // Cancelled at once, while the worker is still held at the gate.
    assert(text(cancelled.body["status"]) == "cancelled");
    const auto never_started = wait_for(server, queued_id, "cancelled");
    assert(never_started["progress"]["evaluations"].u() == 0);

    const auto solution = send(
        server,
        crow::HTTPMethod::GET,
        "/assignment/runs/" + queued_id + "/solution");
    assert(solution.code == 409);
    assert(text(solution.body["error"]["code"]) == "result_not_ready");

    // It left its place in the queue, and can be forgotten.
    const auto replacement = submit(server, "gated", R"({"input": {}})");
    assert(replacement.code == 202);
    assert(
        send(server, crow::HTTPMethod::Delete, "/assignment/runs/" + queued_id).code
        == 204);
    assert(
        send(server, crow::HTTPMethod::GET, "/assignment/runs/" + queued_id).code == 404);

    run_gate.open();
    wait_for(server, running, "succeeded");
    wait_for(server, text(replacement.body["id"]), "succeeded");
}

// Destroying the blueprint stops its runs: the running one stops, the queued
// one never starts, and the destructor does not wait for either to end alone.
void destroying_the_blueprint_stops_its_runs()
{
    const auto started = std::chrono::steady_clock::now();
    {
        auto api = easylocal::rest::blueprint(
            "/stopping",
            make_application(),
            AssignmentCodec{},
            easylocal::rest::blueprint_options{.workers = 1, .queue_capacity = 1});
        crow::SimpleApp server;
        server.loglevel(crow::LogLevel::Warning);
        server.register_blueprint(api.crow_blueprint());
        server.add_blueprint();
        server.validate();

        const auto start = [&server] {
            return send(
                server,
                crow::HTTPMethod::POST,
                "/stopping/runners/endless/runs",
                R"({"input": {}})");
        };
        const auto running = start();
        assert(running.code == 202);
        const auto status_url = "/stopping/runs/" + text(running.body["id"]);
        while (text(send(server, crow::HTTPMethod::GET, status_url).body["status"])
            != "running")
            std::this_thread::sleep_for(1ms);
        assert(start().code == 202); // queued behind it
    }
    assert(std::chrono::steady_clock::now() - started < 10s);
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

// A service with a max_timeout: longer runs are rejected, and a run without a
// timeout gets it.
void a_run_is_no_longer_than_the_max_timeout()
{
    auto api = easylocal::rest::blueprint(
        "/bounded",
        make_application(),
        AssignmentCodec{},
        easylocal::rest::blueprint_options{.workers = 1, .max_timeout = 5.0});
    crow::SimpleApp server;
    server.loglevel(crow::LogLevel::Warning);
    server.register_blueprint(api.crow_blueprint());
    server.add_blueprint();
    server.validate();
    const auto post = [&server](std::string body) {
        return send(
            server,
            crow::HTTPMethod::POST,
            "/bounded/runners/fi/runs",
            std::move(body));
    };

    const auto longer = post(R"({"input": {}, "timeout": 10})");
    assert(longer.code == 422);
    assert(text(longer.body["error"]["code"]) == "invalid_run_request");
    assert(text(longer.body["error"]["message"]).starts_with("'timeout' is longer"));

    const auto bounded = post(R"({"input": {}})");
    assert(bounded.code == 202);
    assert(bounded.body["timeout"].d() == 5.0);
    const auto shorter = post(R"({"input": {}, "timeout": 1.5})");
    assert(shorter.code == 202);
    assert(shorter.body["timeout"].d() == 1.5);
    wait_for_run(server, "/bounded/runs/" + text(bounded.body["id"]), "succeeded");
    wait_for_run(server, "/bounded/runs/" + text(shorter.body["id"]), "succeeded");
    assert(
        send(server, crow::HTTPMethod::GET, "/bounded/").body["max_timeout"].d() == 5.0);

    bool rejected = false;
    try
    {
        [[maybe_unused]] auto negative = easylocal::rest::blueprint(
            "/negative",
            make_application(),
            AssignmentCodec{},
            easylocal::rest::blueprint_options{.max_timeout = -1.0});
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    assert(rejected);
}

void a_zero_completed_run_capacity_is_rejected()
{
    bool rejected = false;
    try
    {
        [[maybe_unused]] auto api = easylocal::rest::blueprint(
            "/assignment",
            make_application(),
            AssignmentCodec{},
            easylocal::rest::blueprint_options{.completed_run_capacity = 0});
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
    a_run_has_a_time_limit(server);
    a_deeply_nested_body_is_rejected_before_parsing(server);
    a_pipeline_runs_by_name(server);
    a_run_stops_at_its_target(server);
    a_run_has_its_own_parameters(server);
    a_run_whose_response_fails_is_not_registered(server);
    json_parameter_values_become_text();
    an_arithmetic_target_is_a_number();
    a_problem_with_text_hooks_needs_no_codec();
    a_full_queue_rejects_runs_and_a_queued_run_can_be_cancelled(server);
    an_empty_prefix_is_rejected();
    a_zero_completed_run_capacity_is_rejected();
    a_run_is_no_longer_than_the_max_timeout();
    destroying_the_blueprint_stops_its_runs();
    run_gate.open(); // never leave a worker waiting on exit
}
