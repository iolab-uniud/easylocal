#pragma once

#include <easylocal/adapters/rest/execution.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/cost/concepts.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/runners/search_run.hpp>

#include <crow.h>

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
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

struct blueprint_options
{
    std::size_t workers{default_worker_count()};
    std::size_t queue_capacity{64};
    std::size_t completed_run_capacity{64};
    // Base seed of the RNG given to stochastic runners: a run without an
    // explicit "seed" uses seed + its run id, so runs differ but are
    // reproducible.
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

} // namespace detail

template<class App, class Codec>
    requires std::copy_constructible<App> &&
             std::move_constructible<Codec> &&
             detail::application_codec<Codec, App>
class app_blueprint
{
public:
    using app_type = App;
    using codec_type = Codec;
    using input_type = typename App::input_type;
    using bound_app_type = detail::bound_app_t<App>;
    using solution_manager_type = typename bound_app_type::solution_manager_type;
    using solution_type = typename solution_manager_type::solution_type;
    using cost_type = typename solution_manager_type::cost_type;
    using session_type = easylocal::Session<App>;

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

    [[nodiscard]] crow::Blueprint& crow_blueprint() noexcept
    {
        return blueprint_;
    }

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
        mutable std::mutex mutex;
        std::stop_source stop_source;
        run_state state{run_state::queued};
        std::optional<solution_type> solution;
        std::optional<cost_type> cost;
        std::string error;
        std::atomic<std::size_t> evaluations{};
        std::atomic<std::size_t> iterations{};
        std::atomic<std::size_t> evaluation_limit{};
        std::atomic_bool has_evaluation_limit{};
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
        application_.for_each_runner_registration(
            [&]<class Algorithm>(
                const std::string_view name,
                const typename Algorithm::parameters_type&) {
                found = found || name == requested;
            });
        return found;
    }

    [[nodiscard]] std::vector<std::string> runner_names() const
    {
        std::vector<std::string> names;
        const std::lock_guard lock{application_mutex_};
        names.reserve(App::runner_count);
        application_.for_each_runner_registration(
            [&]<class Algorithm>(
                const std::string_view name,
                const typename Algorithm::parameters_type&) {
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

    // The target of a run: by the codec's decode_cost, or as a number for an
    // arithmetic cost.
    [[nodiscard]] cost_type decode_target(const crow::json::rvalue& payload) const
    {
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
        body["status"] = std::string{state_name(record->state)};
        body["cancellation_requested"] = record->stop_source.stop_requested();
        body["progress"]["evaluations"] = static_cast<std::uint64_t>(
            record->evaluations.load(std::memory_order_relaxed));
        body["progress"]["iterations"] = static_cast<std::uint64_t>(
            record->iterations.load(std::memory_order_relaxed));
        if (record->has_evaluation_limit.load(std::memory_order_relaxed))
        {
            body["progress"]["evaluation_limit"] = static_cast<std::uint64_t>(
                record->evaluation_limit.load(std::memory_order_relaxed));
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
                target.emplace(decode_target(payload["target"]));

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
            session.set_seed(record->seed);
            {
                const std::lock_guard lock{runs_mutex_};
                runs_.emplace(id, record);
            }

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
                        record->evaluations.store(
                            progress.evaluations,
                            std::memory_order_relaxed);
                        record->iterations.store(
                            progress.iterations,
                            std::memory_order_relaxed);
                        record->has_evaluation_limit.store(
                            progress.evaluation_limit.has_value(),
                            std::memory_order_relaxed);
                        record->evaluation_limit.store(
                            progress.evaluation_limit.value_or(0),
                            std::memory_order_relaxed);
                    };
                    const easylocal::run_control control{
                        record->stop_source.get_token(),
                        observer};

                    try
                    {
                        const bool ran = record->target
                            ? session.run(
                                  runner,
                                  easylocal::with(control).stop_at(*record->target))
                            : session.run(runner, easylocal::with(control));

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

            crow::json::wvalue body;
            body["id"] = id;
            body["runner"] = runner;
            body["seed"] = record->seed;
            if (record->target)
                body["target"] = encode_cost(*record->target);
            body["status"] = "queued";
                body["cancellation_requested"] = false;
            body["progress"]["evaluations"] = std::uint64_t{0};
            body["progress"]["iterations"] = std::uint64_t{0};
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
            return detail::error_response(
                404,
                "run_not_found",
                "run '" + id + "' does not exist");
        }
        return detail::json_response(200, run_body(record));
    }

    [[nodiscard]] crow::response run_solution(const std::string& id) const
    {
        const auto record = find_run(id);
        if (!record)
        {
            return detail::error_response(
                404,
                "run_not_found",
                "run '" + id + "' does not exist");
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
            return detail::error_response(
                404,
                "run_not_found",
                "run '" + id + "' does not exist");
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
            return detail::error_response(
                404,
                "run_not_found",
                "run '" + id + "' does not exist");
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
        .methods(crow::HTTPMethod::DELETE)
        ([this](std::string id) {
            return delete_run(id);
        });
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

template<class App, class Codec>
[[nodiscard]] auto blueprint(
    std::string prefix,
    App application,
    Codec codec,
    blueprint_options options = {})
{
    return app_blueprint<std::remove_cvref_t<App>, std::remove_cvref_t<Codec>>{
        std::move(prefix),
        std::move(application),
        std::move(codec),
        options,
    };
}

} // namespace easylocal::rest
