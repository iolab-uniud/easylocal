#pragma once

/// \file
/// app_blueprint: a Crow blueprint that exposes an app over HTTP
/// (docs/rest.md).
///
/// It lists the runners and the parameters, submits runs (each on its own
/// Session, with optional parameter overrides, target and initial solution),
/// reports their state and solution, and cancels them. A codec of the problem
/// turns its Input, Solution and costs into JSON and back.

#include <easylocal/adapters/rest/execution.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/config/overrides.hpp>
#include <easylocal/cost/concepts.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/runners/search_run.hpp>

#include <crow.h>

#include <atomic>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace easylocal::rest
{

/// The options of an app_blueprint: its execution pool and run history.
struct blueprint_options
{
    /// The number of worker threads that execute runs (default: one less than
    /// the hardware threads, at least 1).
    std::size_t workers{default_worker_count()};
    /// The maximum number of runs waiting for a worker: a submission beyond it
    /// is rejected with `503`.
    std::size_t queue_capacity{64};
    /// The maximum number of terminal runs kept, the oldest forgotten first;
    /// must be positive.
    std::size_t completed_run_capacity{64};
    /// Base seed of the RNG given to stochastic runners: a run without an
    /// explicit "seed" uses seed + its run id, so runs differ but are
    /// reproducible.
    std::uint64_t seed{0};
};

namespace detail
{

template<class App>
using bound_app_t = decltype(std::declval<const App&>().bind(
    std::declval<const typename App::input_type&>()));

template<class App>
using app_solution_manager_t = typename bound_app_t<App>::solution_manager_type;

template<class App>
using app_solution_t = typename app_solution_manager_t<App>::solution_type;

template<class App>
using app_cost_t = typename app_solution_manager_t<App>::cost_type;

template<class Codec, class App>
concept application_codec =
    requires(
        const Codec& codec,
        const crow::json::rvalue& payload,
        const typename App::input_type& input,
        const app_solution_t<App>& solution,
        const app_cost_t<App>& cost) {
        {
            codec.decode_input(payload)
        } -> std::convertible_to<typename App::input_type>;
        {
            codec.encode_solution(input, solution)
        } -> std::same_as<crow::json::wvalue>;
        {
            codec.encode_cost(cost)
        } -> std::same_as<crow::json::wvalue>;
    };

template<class Codec, class App>
concept decodes_initial_solution =
    requires(
        const Codec& codec,
        const typename App::input_type& input,
        const crow::json::rvalue& payload) {
        {
            codec.decode_initial_solution(input, payload)
        } -> std::convertible_to<app_solution_t<App>>;
    };

template<class Codec, class App>
concept decodes_cost = requires(const Codec& codec, const crow::json::rvalue& payload) {
    { codec.decode_cost(payload) } -> std::convertible_to<app_cost_t<App>>;
};

// The target of a run with an arithmetic cost: a JSON number, an integer for
// integral costs.
template<cost::arithmetic Cost>
[[nodiscard]] Cost decode_arithmetic_cost(const crow::json::rvalue& payload)
{
    if (payload.t() != crow::json::type::Number)
        throw std::invalid_argument{"'target' must be a number"};
    if constexpr (std::integral<Cost>)
    {
        switch (payload.nt())
        {
        case crow::json::num_type::Signed_integer:
            return static_cast<Cost>(payload.i());
        case crow::json::num_type::Unsigned_integer:
            return static_cast<Cost>(payload.u());
        default:
            throw std::invalid_argument{"'target' must be an integer"};
        }
    }
    else
    {
        return static_cast<Cost>(payload.d());
    }
}

[[nodiscard]] inline std::string normalize_prefix(std::string prefix)
{
    while (!prefix.empty() && prefix.front() == '/')
    {
        prefix.erase(prefix.begin());
    }
    while (!prefix.empty() && prefix.back() == '/')
    {
        prefix.pop_back();
    }
    if (prefix.empty())
    {
        throw std::invalid_argument{"REST blueprint prefix must not be empty"};
    }
    return prefix;
}

[[nodiscard]] inline crow::response json_response(
    const int status,
    crow::json::wvalue body)
{
    return crow::response{status, std::move(body)};
}

[[nodiscard]] inline crow::response error_response(
    const int status,
    std::string code,
    std::string message)
{
    crow::json::wvalue body;
    body["error"]["code"] = std::move(code);
    body["error"]["message"] = std::move(message);
    return json_response(status, std::move(body));
}

// The deepest nesting of arrays and objects a request body may have: Crow's
// recursive parser could otherwise exhaust the stack of its thread.
inline constexpr std::size_t max_json_depth = 64;

// Whether the arrays and objects of a JSON text nest deeper than limit, by a
// linear scan that skips strings; it does not check the rest of the syntax.
[[nodiscard]] inline bool json_nests_deeper_than(
    const std::string_view text,
    const std::size_t limit) noexcept
{
    std::size_t depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (const char c : text)
    {
        if (in_string)
        {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                in_string = false;
        }
        else if (c == '"')
            in_string = true;
        else if (c == '[' || c == '{')
        {
            if (++depth > limit)
                return true;
        }
        else if ((c == ']' || c == '}') && depth > 0)
            --depth;
    }
    return false;
}

// The answer about a run that does not exist, or no longer does.
[[nodiscard]] inline crow::response run_not_found(const std::string& id)
{
    return error_response(404, "run_not_found", "run '" + id + "' does not exist");
}

// A JSON parameter value as the text a parameter_set parses: numbers in their
// shortest exact form, true/false, strings verbatim, [a, b] for arrays.
[[nodiscard]] inline std::string parameter_text(
    const crow::json::rvalue& value,
    const std::string& path)
{
    switch (value.t())
    {
    case crow::json::type::Number:
        switch (value.nt())
        {
        case crow::json::num_type::Signed_integer:
            return std::to_string(value.i());
        case crow::json::num_type::Unsigned_integer:
            return std::to_string(value.u());
        default:
            return config::format_value(value.d());
        }
    case crow::json::type::True:
        return "true";
    case crow::json::type::False:
        return "false";
    case crow::json::type::String:
        return std::string{value.s()};
    case crow::json::type::List:
    {
        std::string text{"["};
        for (const auto& element : value)
        {
            if (element.t() == crow::json::type::List
                || element.t() == crow::json::type::Object)
                throw std::invalid_argument{
                    "parameter '" + path + "': array elements must be values"};
            if (text.size() > 1)
                text += ", ";
            text += parameter_text(element, path);
        }
        return text + "]";
    }
    default:
        throw std::invalid_argument{
            "parameter '" + path
            + "': expected a number, a boolean, a string or an array"};
    }
}

// The "parameters" of a run request as path = value overrides. Nested objects
// and dotted keys compose, so {"runners": {"sa": {"temperature.cooling_rate":
// 0.9}}} and {"runners.sa.temperature.cooling_rate": 0.9} are the same. Its
// recursion is bounded by max_json_depth, which submit_run checks first.
inline void collect_parameters(
    const crow::json::rvalue& value,
    const std::string& path,
    std::vector<config::owned_text_override>& overrides)
{
    if (value.t() != crow::json::type::Object)
    {
        if (path.empty())
            throw std::invalid_argument{"'parameters' must be an object"};
        overrides.push_back({path, parameter_text(value, path)});
        return;
    }
    for (const auto& entry : value)
    {
        const std::string key{entry.key()};
        collect_parameters(entry, path.empty() ? key : path + "." + key, overrides);
    }
}

} // namespace detail

/// A Crow blueprint that serves an app over HTTP, with its runs executed by a
/// pool of worker threads.
///
/// Its routes list the runners and the parameters, submit a run of a runner
/// (each on its own Session, with optional seed, parameter overrides, target
/// and initial solution), report its state, progress and solution, cancel it
/// and forget it. It is neither copyable nor movable, since its routes refer
/// to it: keep it alive as long as the Crow app uses its blueprint. Calls to
/// the codec are serialized.
///
/// Requires a copyable app and a codec with `decode_input(json)`,
/// `encode_solution(input, solution)` and `encode_cost(cost)`, and optionally
/// `decode_initial_solution(input, json)` and `decode_cost(json)`.
template<class App, class Codec>
    requires std::copy_constructible<App> &&
             std::move_constructible<Codec> &&
             detail::application_codec<Codec, App>
class app_blueprint
{
public:
    /// The app served.
    using app_type = App;
    /// The codec that turns the problem's values into JSON and back.
    using codec_type = Codec;
    /// The Input of the problem.
    using input_type = typename App::input_type;
    /// The app bound to an Input, as `bind()` returns it.
    using bound_app_type = detail::bound_app_t<App>;
    /// The solution manager of the bound app.
    using solution_manager_type = typename bound_app_type::solution_manager_type;
    /// The Solution of the problem.
    using solution_type = typename solution_manager_type::solution_type;
    /// The cost of a solution.
    using cost_type = typename solution_manager_type::cost_type;
    /// The Session that executes one run.
    using session_type = easylocal::Session<App>;

    /// From the prefix of its routes, the app, the codec and the options.
    ///
    /// Leading and trailing slashes of the prefix are dropped. Throws
    /// `std::invalid_argument` when the prefix is empty or
    /// `completed_run_capacity` is zero.
    app_blueprint(
        std::string prefix,
        App application,
        Codec codec,
        blueprint_options options = {})
        : application_{std::move(application)},
          codec_{std::move(codec)},
          options_{options},
          prefix_{detail::normalize_prefix(std::move(prefix))},
          blueprint_{prefix_},
          execution_{options_.workers, options_.queue_capacity}
    {
        if (options_.completed_run_capacity == 0)
        {
            throw std::invalid_argument{
                "REST completed_run_capacity must be greater than zero"};
        }
        register_routes();
    }

    app_blueprint(const app_blueprint&) = delete;
    app_blueprint& operator=(const app_blueprint&) = delete;
    app_blueprint(app_blueprint&&) = delete;
    app_blueprint& operator=(app_blueprint&&) = delete;

    /// Stops the runs: those queued end cancelled without starting, those
    /// running stop at their next check; then waits for them.
    ~app_blueprint()
    {
        const std::lock_guard lock{runs_mutex_};
        for (auto& entry : runs_)
            entry.second->stop_source.request_stop();
    }

    /// The Crow blueprint, to register on a Crow app.
    [[nodiscard]] crow::Blueprint& crow_blueprint() noexcept
    {
        return blueprint_;
    }

    /// The prefix of the routes, without leading and trailing slashes.
    [[nodiscard]] std::string_view prefix() const noexcept
    {
        return prefix_;
    }

private:
    enum class run_state
    {
        queued,
        running,
        succeeded,
        cancelled,
        failed,
    };

    struct run_record
    {
        std::string id;
        std::string runner;
        std::shared_ptr<const input_type> input;
        std::uint64_t seed{};
        std::optional<cost_type> target;
        std::optional<double> timeout;                       // seconds
        std::optional<std::size_t> max_evaluations;
        std::vector<config::owned_text_override> parameters; // as requested
        mutable std::mutex mutex;
        std::stop_source stop_source;
        run_state state{run_state::queued};
        std::optional<solution_type> solution;
        std::optional<cost_type> cost;
        std::string error;
        easylocal::shared_run_progress progress;
    };

    [[nodiscard]] static std::string_view state_name(const run_state state) noexcept
    {
        switch (state)
        {
        case run_state::queued:
            return "queued";
        case run_state::running:
            return "running";
        case run_state::succeeded:
            return "succeeded";
        case run_state::cancelled:
            return "cancelled";
        case run_state::failed:
            return "failed";
        }
        return "unknown";
    }

    [[nodiscard]] App copy_application() const
    {
        const std::lock_guard lock{application_mutex_};
        return application_;
    }

    [[nodiscard]] std::string application_name() const
    {
        const std::lock_guard lock{application_mutex_};
        return std::string{application_.name()};
    }

    [[nodiscard]] input_type decode_input(const crow::json::rvalue& payload) const
    {
        const std::lock_guard lock{codec_mutex_};
        return codec_.decode_input(payload);
    }

    [[nodiscard]] crow::json::wvalue encode_solution(
        const input_type& input,
        const solution_type& solution) const
    {
        const std::lock_guard lock{codec_mutex_};
        return codec_.encode_solution(input, solution);
    }

    [[nodiscard]] crow::json::wvalue encode_cost(const cost_type& cost) const
    {
        const std::lock_guard lock{codec_mutex_};
        return codec_.encode_cost(cost);
    }

    [[nodiscard]] bool runner_exists(const std::string_view requested) const
    {
        bool found = false;
        const std::lock_guard lock{application_mutex_};
        application_.for_each_registration_name([&](const std::string_view name) {
            found = found || name == requested;
        });
        return found;
    }

    [[nodiscard]] std::vector<std::string> runner_names() const
    {
        std::vector<std::string> names;
        const std::lock_guard lock{application_mutex_};
        names.reserve(App::runner_count);
        application_.for_each_registration_name([&](const std::string_view name) {
            names.emplace_back(name);
        });
        return names;
    }

    [[nodiscard]] std::shared_ptr<run_record> find_run(const std::string& id) const
    {
        const std::lock_guard lock{runs_mutex_};
        const auto found = runs_.find(id);
        return found == runs_.end() ? nullptr : found->second;
    }

    [[nodiscard]] solution_type decode_initial_solution(
        const input_type& input,
        const crow::json::rvalue& payload) const
        requires detail::decodes_initial_solution<Codec, App>
    {
        const std::lock_guard lock{codec_mutex_};
        return codec_.decode_initial_solution(input, payload);
    }

    // The time limit of a run, in seconds: a non-negative number.
    [[nodiscard]] static double decode_timeout(const crow::json::rvalue& payload)
    {
        if (payload.t() != crow::json::type::Number)
            throw std::invalid_argument{"'timeout' must be a number of seconds"};
        const double seconds = payload.d();
        if (!(seconds >= 0.0) || !std::isfinite(seconds))
            throw std::invalid_argument{
                "'timeout' must be a non-negative number of seconds"};
        return seconds;
    }

    // The evaluation budget of a run: a non-negative integer.
    [[nodiscard]] static std::size_t decode_max_evaluations(
        const crow::json::rvalue& payload)
    {
        if (payload.t() != crow::json::type::Number
            || payload.nt() == crow::json::num_type::Floating_point
            || (payload.nt() == crow::json::num_type::Signed_integer && payload.i() < 0))
        {
            throw std::invalid_argument{
                "'max_evaluations' must be a non-negative integer"};
        }
        return static_cast<std::size_t>(payload.u());
    }

    // The target of a run: a string in the textual syntax of costs, read by
    // the problem's read_cost or cost::from_text; otherwise by the codec's
    // decode_cost, or as a number for an arithmetic cost.
    [[nodiscard]] cost_type decode_target(
        const crow::json::rvalue& payload,
        const input_type& input) const
    {
        if constexpr (readable_cost<input_type, cost_type>)
        {
            if (payload.t() == crow::json::type::String)
            {
                try
                {
                    return easylocal::read_cost<cost_type>(
                        input,
                        std::string{payload.s()});
                }
                catch (const std::invalid_argument& error)
                {
                    throw std::invalid_argument{"'target': " + std::string{error.what()}};
                }
            }
        }
        if constexpr (detail::decodes_cost<Codec, App>)
        {
            const std::lock_guard lock{codec_mutex_};
            return codec_.decode_cost(payload);
        }
        else if constexpr (cost::arithmetic<cost_type>)
        {
            return detail::decode_arithmetic_cost<cost_type>(payload);
        }
        else
        {
            throw std::invalid_argument{
                "'target' is not supported: the application codec does not decode "
                "costs (decode_cost)"};
        }
    }

    // The parameters a run was submitted with, by path, as text.
    static void add_parameters(crow::json::wvalue& body, const run_record& record)
    {
        for (const auto& parameter : record.parameters)
            body["parameters"][parameter.path] = parameter.value;
    }

    [[nodiscard]] static bool is_terminal(const run_state state) noexcept
    {
        return state == run_state::succeeded ||
               state == run_state::cancelled ||
               state == run_state::failed;
    }

    [[nodiscard]] std::string run_url(const std::string_view id) const
    {
        return "/" + prefix_ + "/runs/" + std::string{id};
    }

    [[nodiscard]] crow::json::wvalue run_body(
        const std::shared_ptr<run_record>& record) const
    {
        crow::json::wvalue body;
        const std::lock_guard lock{record->mutex};
        body["id"] = record->id;
        body["runner"] = record->runner;
        body["seed"] = record->seed;
        if (record->target)
            body["target"] = encode_cost(*record->target);
        if (record->timeout)
            body["timeout"] = *record->timeout;
        if (record->max_evaluations)
            body["max_evaluations"] =
                static_cast<std::uint64_t>(*record->max_evaluations);
        add_parameters(body, *record);
        body["status"] = std::string{state_name(record->state)};
        body["cancellation_requested"] = record->stop_source.stop_requested();
        const auto progress = record->progress.load();
        body["progress"]["evaluations"] =
            static_cast<std::uint64_t>(progress.evaluations);
        body["progress"]["iterations"] = static_cast<std::uint64_t>(progress.iterations);
        if (progress.evaluation_limit)
        {
            body["progress"]["evaluation_limit"] =
                static_cast<std::uint64_t>(*progress.evaluation_limit);
        }
        if (!record->error.empty())
        {
            body["error"]["code"] = "run_failed";
            body["error"]["message"] = record->error;
        }
        if ((record->state == run_state::succeeded ||
             record->state == run_state::cancelled) &&
            record->solution.has_value())
        {
            body["solution_url"] = run_url(record->id) + "/solution";
        }
        return body;
    }

    void remember_completed(const std::string& id)
    {
        const std::lock_guard lock{runs_mutex_};
        if (!runs_.contains(id))
        {
            return;
        }

        completed_runs_.push_back(id);
        while (completed_runs_.size() > options_.completed_run_capacity)
        {
            const auto expired = std::move(completed_runs_.front());
            completed_runs_.pop_front();
            runs_.erase(expired);
        }
    }

    // The session's first current solution: the one in the request, decoded,
    // or the initial solution of the SolutionManager.
    void set_initial_solution(
        session_type& session,
        const crow::json::rvalue* payload) const
    {
        if (payload != nullptr)
        {
            if constexpr (detail::decodes_initial_solution<Codec, App>)
            {
                session.set_solution(decode_initial_solution(session.input(), *payload));
            }
            else
            {
                throw std::invalid_argument{
                    "initial_solution is not supported by this application codec"};
            }
        }
        else if constexpr (session_type::supports_initial_solution)
        {
            session.use_initial_solution();
        }
        else
        {
            throw std::invalid_argument{
                "no initial_solution was supplied and this application does not provide initial_solution()"};
        }
    }

    [[nodiscard]] crow::response root_response() const
    {
        crow::json::wvalue body;
        body["application"] = application_name();
        body["runners"] = runner_names();
        body["workers"] = static_cast<std::uint64_t>(execution_.worker_count());
        body["queue_capacity"] = static_cast<std::uint64_t>(execution_.queue_capacity());
        body["completed_run_capacity"] = static_cast<std::uint64_t>(
            options_.completed_run_capacity);
        {
            const std::lock_guard lock{runs_mutex_};
            body["runs"] = static_cast<std::uint64_t>(runs_.size());
            body["completed_runs"] = static_cast<std::uint64_t>(
                completed_runs_.size());
        }
        return detail::json_response(200, std::move(body));
    }

    // The app's parameters, with the values a run uses unless its request
    // changes them.
    [[nodiscard]] crow::response parameters_response() const
    {
        crow::json::wvalue body;
        body["parameters"] = crow::json::wvalue::list{};
        std::size_t index = 0;
        const std::lock_guard lock{application_mutex_};
        for (const auto& parameter : application_.configuration().parameters())
        {
            auto& entry = body["parameters"][index++];
            entry["path"] = parameter.path;
            entry["description"] = std::string{parameter.description};
            entry["value"] = parameter.value;
            entry["read_only"] = parameter.read_only;
        }
        return detail::json_response(200, std::move(body));
    }

    [[nodiscard]] crow::response runners_response() const
    {
        crow::json::wvalue body;
        body["runners"] = runner_names();
        return detail::json_response(200, std::move(body));
    }

    [[nodiscard]] crow::response submit_run(
        const crow::request& request,
        const std::string& runner)
    {
        if (!runner_exists(runner))
        {
            return detail::error_response(
                404,
                "unknown_runner",
                "runner '" + runner + "' is not registered");
        }

        if (detail::json_nests_deeper_than(request.body, detail::max_json_depth))
        {
            return detail::error_response(
                400,
                "invalid_json",
                "request body nests arrays and objects deeper than "
                    + std::to_string(detail::max_json_depth) + " levels");
        }
        auto payload = crow::json::load(request.body);
        if (!payload)
        {
            return detail::error_response(
                400,
                "invalid_json",
                "request body is not valid JSON");
        }
        if (payload.t() != crow::json::type::Object || !payload.has("input"))
        {
            return detail::error_response(
                422,
                "invalid_run_request",
                "request body must be an object containing an 'input' field");
        }

        try
        {
            auto input = std::make_shared<const input_type>(
                decode_input(payload["input"]));
            // The run's own session; its seed is set once the run has an id.
            session_type session{copy_application(), input, 0};
            // The run's parameters, before its initial solution is built.
            std::vector<config::owned_text_override> parameters;
            if (payload.has("parameters"))
            {
                detail::collect_parameters(payload["parameters"], {}, parameters);
                const auto views = config::override_views(parameters);
                if (const auto configured = session.configure(views); !configured)
                {
                    std::string message;
                    for (const auto& diagnostic : configured.diagnostics)
                    {
                        if (!message.empty())
                            message += "; ";
                        message += diagnostic.path + ": " + diagnostic.message;
                    }
                    return detail::error_response(422, "invalid_parameters", message);
                }
            }
            const crow::json::rvalue* initial_payload =
                payload.has("initial_solution")
                    ? &payload["initial_solution"]
                    : nullptr;
            set_initial_solution(session, initial_payload);

            if (payload.has("seed") &&
                (payload["seed"].t() != crow::json::type::Number ||
                 (payload["seed"].nt() != crow::json::num_type::Unsigned_integer &&
                  payload["seed"].nt() != crow::json::num_type::Signed_integer) ||
                 (payload["seed"].nt() == crow::json::num_type::Signed_integer &&
                  payload["seed"].i() < 0)))
            {
                return detail::error_response(
                    422,
                    "invalid_run_request",
                    "'seed' must be a non-negative integer");
            }

            std::optional<cost_type> target;
            if (payload.has("target"))
                target.emplace(decode_target(payload["target"], *input));
            std::optional<double> timeout;
            if (payload.has("timeout"))
                timeout.emplace(decode_timeout(payload["timeout"]));
            std::optional<std::size_t> max_evaluations;
            if (payload.has("max_evaluations"))
                max_evaluations.emplace(
                    decode_max_evaluations(payload["max_evaluations"]));

            const auto run_number = next_run_id_.fetch_add(1);
            const auto id = std::to_string(run_number);
            auto record = std::make_shared<run_record>();
            record->id = id;
            record->seed = payload.has("seed")
                ? static_cast<std::uint64_t>(payload["seed"].u())
                : options_.seed + static_cast<std::uint64_t>(run_number);
            record->runner = runner;
            record->input = input;
            record->target = target;
            record->timeout = timeout;
            record->max_evaluations = max_evaluations;
            record->parameters = std::move(parameters);
            session.set_seed(record->seed);
            {
                const std::lock_guard lock{runs_mutex_};
                runs_.emplace(id, record);
            }

            // The body of the response, before the run can start: queued.
            auto body = run_body(record);
            const bool accepted = execution_.try_submit(
                [this, session = std::move(session), record, runner]() mutable {
                    bool cancelled_before_start = false;
                    {
                        const std::lock_guard lock{record->mutex};
                        if (record->stop_source.stop_requested())
                        {
                            record->state = run_state::cancelled;
                            cancelled_before_start = true;
                        }
                        else
                        {
                            record->state = run_state::running;
                        }
                    }
                    if (cancelled_before_start)
                    {
                        remember_completed(record->id);
                        return;
                    }

                    auto observer = [record](const easylocal::run_progress& progress) {
                        record->progress.store(progress);
                    };
                    const easylocal::run_control control{
                        record->stop_source.get_token(),
                        observer};

                    try
                    {
                        auto options = easylocal::with(control);
                        if (record->timeout)
                            options = options.timeout(*record->timeout);
                        if (record->max_evaluations)
                            options = options.max_evaluations(*record->max_evaluations);
                        const bool ran = record->target
                            ? session.run(runner, options.stop_at(*record->target))
                            : session.run(runner, options);

                        const std::lock_guard lock{record->mutex};
                        if (ran)
                        {
                            record->solution.emplace(session.solution());
                            record->cost.emplace(session.evaluate());
                            record->state = record->stop_source.stop_requested()
                                ? run_state::cancelled
                                : run_state::succeeded;
                        }
                        else
                        {
                            record->state = run_state::failed;
                            record->error = "runner disappeared from application snapshot";
                        }
                    }
                    catch (const std::exception& error)
                    {
                        const std::lock_guard lock{record->mutex};
                        record->state = run_state::failed;
                        record->error = error.what();
                    }
                    catch (...)
                    {
                        const std::lock_guard lock{record->mutex};
                        record->state = run_state::failed;
                        record->error = "unknown runner error";
                    }

                    remember_completed(record->id);
                });

            if (!accepted)
            {
                const std::lock_guard lock{runs_mutex_};
                runs_.erase(id);
                return detail::error_response(
                    503,
                    "queue_full",
                    "runner execution queue is full");
            }

            auto response = detail::json_response(202, std::move(body));
            response.set_header("Location", run_url(id));
            return response;
        }
        catch (const std::invalid_argument& error)
        {
            return detail::error_response(
                422,
                "invalid_run_request",
                error.what());
        }
        catch (const std::exception& error)
        {
            return detail::error_response(
                500,
                "internal_error",
                "cannot create run: " + std::string{error.what()});
        }
        catch (...)
        {
            return detail::error_response(
                500,
                "internal_error",
                "cannot create run: unknown error");
        }
    }

    [[nodiscard]] crow::response run_status(const std::string& id) const
    {
        const auto record = find_run(id);
        if (!record)
        {
            return detail::run_not_found(id);
        }
        return detail::json_response(200, run_body(record));
    }

    [[nodiscard]] crow::response run_solution(const std::string& id) const
    {
        const auto record = find_run(id);
        if (!record)
        {
            return detail::run_not_found(id);
        }

        const std::lock_guard lock{record->mutex};
        if (record->state == run_state::failed)
        {
            return detail::error_response(
                409,
                "run_failed",
                record->error.empty() ? "run failed" : record->error);
        }
        if ((record->state != run_state::succeeded &&
             record->state != run_state::cancelled) ||
            !record->solution)
        {
            return detail::error_response(
                409,
                "result_not_ready",
                "run has not produced a solution yet");
        }

        crow::json::wvalue body;
        body["id"] = record->id;
        body["runner"] = record->runner;
        body["seed"] = record->seed;
        if (record->target)
            body["target"] = encode_cost(*record->target);
        if (record->timeout)
            body["timeout"] = *record->timeout;
        if (record->max_evaluations)
            body["max_evaluations"] =
                static_cast<std::uint64_t>(*record->max_evaluations);
        add_parameters(body, *record);
        body["status"] = std::string{state_name(record->state)};
        body["cost"] = encode_cost(*record->cost);
        body["solution"] = encode_solution(
            *record->input,
            *record->solution);
        return detail::json_response(200, std::move(body));
    }

    [[nodiscard]] crow::response cancel_run(const std::string& id)
    {
        const auto record = find_run(id);
        if (!record)
        {
            return detail::run_not_found(id);
        }

        {
            const std::lock_guard lock{record->mutex};
            if (is_terminal(record->state))
            {
                return detail::error_response(
                    409,
                    "run_not_active",
                    "run is already terminal");
            }
            record->stop_source.request_stop();
        }

        return detail::json_response(202, run_body(record));
    }

    [[nodiscard]] crow::response delete_run(const std::string& id)
    {
        const auto record = find_run(id);
        if (!record)
        {
            return detail::run_not_found(id);
        }

        {
            const std::lock_guard lock{record->mutex};
            if (!is_terminal(record->state))
            {
                return detail::error_response(
                    409,
                    "run_not_terminal",
                    "only terminal runs can be deleted");
            }
        }

        {
            const std::lock_guard lock{runs_mutex_};
            runs_.erase(id);
            for (auto it = completed_runs_.begin(); it != completed_runs_.end(); ++it)
            {
                if (*it == id)
                {
                    completed_runs_.erase(it);
                    break;
                }
            }
        }
        return crow::response{204};
    }

    void register_routes()
    {
        CROW_BP_ROUTE(blueprint_, "/")
        ([this] {
            return root_response();
        });

        CROW_BP_ROUTE(blueprint_, "/runners")
        ([this] {
            return runners_response();
        });

        CROW_BP_ROUTE(blueprint_, "/parameters")
        ([this] { return parameters_response(); });

        CROW_BP_ROUTE(blueprint_, "/runners/<string>/runs")
        .methods(crow::HTTPMethod::POST)
        ([this](const crow::request& request, std::string runner) {
            return submit_run(request, runner);
        });

        CROW_BP_ROUTE(blueprint_, "/runs/<string>")
        ([this](std::string id) {
            return run_status(id);
        });

        CROW_BP_ROUTE(blueprint_, "/runs/<string>/solution")
        ([this](std::string id) {
            return run_solution(id);
        });

        CROW_BP_ROUTE(blueprint_, "/runs/<string>/cancel")
        .methods(crow::HTTPMethod::POST)
        ([this](std::string id) {
            return cancel_run(id);
        });

        CROW_BP_ROUTE(blueprint_, "/runs/<string>")
            .methods(crow::HTTPMethod::Delete) // DELETE is a Windows macro
            ([this](std::string id) { return delete_run(id); });
    }

    App application_;
    Codec codec_;
    blueprint_options options_;
    std::string prefix_;
    crow::Blueprint blueprint_;

    mutable std::mutex application_mutex_;
    mutable std::mutex codec_mutex_;
    mutable std::mutex runs_mutex_;
    std::unordered_map<std::string, std::shared_ptr<run_record>> runs_;
    std::deque<std::string> completed_runs_;
    std::atomic<std::uint64_t> next_run_id_{1};

    // Keep the executor last: it is destroyed first, draining/joining worker
    // threads before the state they may reference is torn down.
    execution_pool execution_;
};

/// The app_blueprint that serves the app with the codec under the prefix.
///
/// The result is neither copyable nor movable: initialize a variable with it.
template<class App, class Codec>
[[nodiscard]] app_blueprint<App, Codec> blueprint(
    std::string prefix,
    App application,
    Codec codec,
    blueprint_options options = {})
{
    return app_blueprint<App, Codec>{
        std::move(prefix),
        std::move(application),
        std::move(codec),
        options,
    };
}

} // namespace easylocal::rest
