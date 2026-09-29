#pragma once

#include <easylocal/run_control.hpp>
#include <easylocal/rest/execution.hpp>

#include <crow.h>

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
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

struct blueprint_options
{
    std::size_t workers{default_worker_count()};
    std::size_t queue_capacity{64};
};

namespace detail
{

template<class App>
using app_runtime_t = decltype(
    std::declval<const App&>().for_input(
        std::declval<const typename App::input_type&>()));

template<class App>
using app_solution_manager_t = typename app_runtime_t<App>::solution_manager_type;

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
        } -> std::same_as<std::optional<app_solution_t<App>>>;
    };

[[nodiscard]] inline auto normalize_prefix(std::string prefix) -> std::string
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

[[nodiscard]] inline auto json_response(
    const int status,
    crow::json::wvalue body) -> crow::response
{
    return crow::response{status, std::move(body)};
}

[[nodiscard]] inline auto error_response(
    const int status,
    std::string message) -> crow::response
{
    crow::json::wvalue body;
    body["error"] = std::move(message);
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
    using runtime_type = detail::app_runtime_t<App>;
    using solution_manager_type = typename runtime_type::solution_manager_type;
    using solution_type = typename solution_manager_type::solution_type;
    using cost_type = typename solution_manager_type::cost_type;

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
        register_routes();
    }

    app_blueprint(const app_blueprint&) = delete;
    auto operator=(const app_blueprint&) -> app_blueprint& = delete;
    app_blueprint(app_blueprint&&) = delete;
    auto operator=(app_blueprint&&) -> app_blueprint& = delete;

    [[nodiscard]] auto crow_blueprint() noexcept -> crow::Blueprint&
    {
        return blueprint_;
    }

    [[nodiscard]] auto prefix() const noexcept -> std::string_view
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
        mutable std::mutex mutex;
        std::stop_source stop_source;
        bool supports_stop{};
        run_state state{run_state::queued};
        std::optional<solution_type> solution;
        std::string error;
        std::atomic<std::size_t> evaluations{};
        std::atomic<std::size_t> iterations{};
        std::atomic<std::size_t> evaluation_limit{};
        std::atomic_bool has_evaluation_limit{};
    };

    [[nodiscard]] static auto state_name(const run_state state) noexcept
        -> std::string_view
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

    [[nodiscard]] auto copy_application() const -> App
    {
        const std::lock_guard lock{application_mutex_};
        return application_;
    }

    [[nodiscard]] auto application_name() const -> std::string
    {
        const std::lock_guard lock{application_mutex_};
        return std::string{application_.name()};
    }

    [[nodiscard]] auto decode_input(const crow::json::rvalue& payload) const
        -> input_type
    {
        const std::lock_guard lock{codec_mutex_};
        return codec_.decode_input(payload);
    }

    [[nodiscard]] auto decode_initial_solution(
        const input_type& input,
        const crow::json::rvalue& payload) const -> std::optional<solution_type>
        requires detail::decodes_initial_solution<Codec, App>
    {
        const std::lock_guard lock{codec_mutex_};
        return codec_.decode_initial_solution(input, payload);
    }

    [[nodiscard]] auto encode_solution(
        const input_type& input,
        const solution_type& solution) const -> crow::json::wvalue
    {
        const std::lock_guard lock{codec_mutex_};
        return codec_.encode_solution(input, solution);
    }

    [[nodiscard]] auto encode_cost(const cost_type& cost) const
        -> crow::json::wvalue
    {
        const std::lock_guard lock{codec_mutex_};
        return codec_.encode_cost(cost);
    }

    [[nodiscard]] auto runner_exists(const std::string_view requested) const -> bool
    {
        bool found = false;
        const std::lock_guard lock{application_mutex_};
        application_.for_each_runner_registration(
            [&]<class Tag>(
                const std::string_view name,
                const typename Tag::config_type&) {
                found = found || name == requested;
            });
        return found;
    }

    [[nodiscard]] auto runner_names() const -> std::vector<std::string>
    {
        std::vector<std::string> names;
        const std::lock_guard lock{application_mutex_};
        names.reserve(App::runner_count);
        application_.for_each_runner_registration(
            [&]<class Tag>(
                const std::string_view name,
                const typename Tag::config_type&) {
                names.emplace_back(name);
            });
        return names;
    }

    [[nodiscard]] auto find_run(const std::string& id) const
        -> std::shared_ptr<run_record>
    {
        const std::lock_guard lock{runs_mutex_};
        const auto found = runs_.find(id);
        return found == runs_.end() ? nullptr : found->second;
    }

    [[nodiscard]] auto make_initial_solution(
        const App& application,
        const input_type& input,
        const crow::json::rvalue& payload) const -> solution_type
    {
        if constexpr (detail::decodes_initial_solution<Codec, App>)
        {
            auto decoded = decode_initial_solution(input, payload);
            if (decoded)
            {
                return std::move(*decoded);
            }
        }

        auto runtime = application.for_input(input);
        if constexpr (requires {
                          runtime.solution_manager().initial_solution();
                      })
        {
            return runtime.solution_manager().initial_solution();
        }
        else
        {
            throw std::invalid_argument{
                "no initial solution was supplied and this application does not provide initial_solution()"};
        }
    }

    [[nodiscard]] auto root_response() const -> crow::response
    {
        crow::json::wvalue body;
        body["application"] = application_name();
        body["runners"] = runner_names();
        body["workers"] = static_cast<std::uint64_t>(execution_.worker_count());
        body["queue_capacity"] = static_cast<std::uint64_t>(execution_.queue_capacity());
        {
            const std::lock_guard lock{runs_mutex_};
            body["runs"] = static_cast<std::uint64_t>(runs_.size());
        }
        return detail::json_response(200, std::move(body));
    }

    [[nodiscard]] auto runners_response() const -> crow::response
    {
        crow::json::wvalue body;
        body["runners"] = runner_names();
        return detail::json_response(200, std::move(body));
    }

    [[nodiscard]] auto submit_run(
        const crow::request& request,
        const std::string& runner) -> crow::response
    {
        if (!runner_exists(runner))
        {
            return detail::error_response(
                404,
                "runner '" + runner + "' is not registered");
        }

        auto payload = crow::json::load(request.body);
        if (!payload)
        {
            return detail::error_response(400, "request body is not valid JSON");
        }

        try
        {
            auto application = copy_application();
            auto input = std::make_shared<const input_type>(
                decode_input(payload));
            auto initial = make_initial_solution(application, *input, payload);

            const auto id = std::to_string(next_run_id_.fetch_add(1));
            auto record = std::make_shared<run_record>();
            record->id = id;
            record->runner = runner;
            record->input = input;
            application.for_each_runner_registration_indexed(
                [&]<class Tag, std::size_t Index>(
                    const std::string_view registered_name,
                    const typename Tag::config_type&) {
                    if (registered_name == runner)
                    {
                        record->supports_stop = App::template
                            runner_supports_run_control<Index>;
                    }
                });

            {
                const std::lock_guard lock{runs_mutex_};
                runs_.emplace(id, record);
            }

            const bool accepted = execution_.try_submit(
                [application = std::move(application),
                 record,
                 initial = std::move(initial),
                 runner]() mutable {
                    {
                        const std::lock_guard lock{record->mutex};
                        if (record->stop_source.stop_requested())
                        {
                            record->state = run_state::cancelled;
                            return;
                        }
                        record->state = run_state::running;
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
                        bool found = false;
                        application.for_each_runner_registration_indexed(
                            [&]<class Tag, std::size_t Index>(
                                const std::string_view registered_name,
                                const typename Tag::config_type&) {
                                if (found || registered_name != runner)
                                {
                                    return;
                                }

                                auto consume_result = [&](auto result) {
                                    static_assert(
                                        requires {
                                            { std::move(result.solution) }
                                                -> std::convertible_to<solution_type>;
                                        },
                                        "REST requires runner results to expose a solution member");

                                    const std::lock_guard lock{record->mutex};
                                    record->solution.emplace(
                                        std::move(result.solution));
                                    record->state =
                                        record->stop_source.stop_requested() &&
                                                record->supports_stop
                                            ? run_state::cancelled
                                            : run_state::succeeded;
                                };

                                if constexpr (App::template
                                                  runner_supports_run_control<Index>)
                                {
                                    consume_result(
                                        application.template run_controlled_at<Index>(
                                            *record->input,
                                            std::move(initial),
                                            control));
                                }
                                else
                                {
                                    consume_result(application.template run_at<Index>(
                                        *record->input,
                                        std::move(initial)));
                                }
                                found = true;
                            });

                        if (!found)
                        {
                            const std::lock_guard lock{record->mutex};
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
                });

            if (!accepted)
            {
                const std::lock_guard lock{runs_mutex_};
                runs_.erase(id);
                return detail::error_response(
                    503,
                    "runner execution queue is full");
            }

            crow::json::wvalue body;
            body["run_id"] = id;
            body["runner"] = runner;
            body["status"] = "queued";
            body["stoppable"] = record->supports_stop;
            body["url"] = "/" + prefix_ + "/runs/" + id;
            return detail::json_response(202, std::move(body));
        }
        catch (const std::invalid_argument& error)
        {
            return detail::error_response(422, error.what());
        }
        catch (const std::exception& error)
        {
            return detail::error_response(
                422,
                "cannot create run: " + std::string{error.what()});
        }
    }

    [[nodiscard]] auto run_status(const std::string& id) const -> crow::response
    {
        const auto record = find_run(id);
        if (!record)
        {
            return detail::error_response(404, "run '" + id + "' does not exist");
        }

        crow::json::wvalue body;
        const std::lock_guard lock{record->mutex};
        body["run_id"] = record->id;
        body["runner"] = record->runner;
        body["status"] = std::string{state_name(record->state)};
        body["stoppable"] = record->supports_stop;
        body["evaluations"] = static_cast<std::uint64_t>(
            record->evaluations.load(std::memory_order_relaxed));
        body["iterations"] = static_cast<std::uint64_t>(
            record->iterations.load(std::memory_order_relaxed));
        if (record->has_evaluation_limit.load(std::memory_order_relaxed))
        {
            body["evaluation_limit"] = static_cast<std::uint64_t>(
                record->evaluation_limit.load(std::memory_order_relaxed));
        }
        if (!record->error.empty())
        {
            body["error"] = record->error;
        }
        if ((record->state == run_state::succeeded ||
             record->state == run_state::cancelled) &&
            record->solution.has_value())
        {
            body["solution_url"] =
                "/" + prefix_ + "/runs/" + record->id + "/solution";
        }
        return detail::json_response(200, std::move(body));
    }

    [[nodiscard]] auto run_solution(const std::string& id) const -> crow::response
    {
        const auto record = find_run(id);
        if (!record)
        {
            return detail::error_response(404, "run '" + id + "' does not exist");
        }

        const auto application = copy_application();
        const std::lock_guard lock{record->mutex};
        if (record->state == run_state::failed)
        {
            return detail::error_response(
                409,
                record->error.empty() ? "run failed" : record->error);
        }
        if ((record->state != run_state::succeeded &&
             record->state != run_state::cancelled) ||
            !record->solution)
        {
            return detail::error_response(409, "run has not produced a solution yet");
        }

        auto runtime = application.for_input(*record->input);
        const auto cost = runtime.solution_manager().evaluate(*record->solution);

        crow::json::wvalue body;
        body["run_id"] = record->id;
        body["runner"] = record->runner;
        body["cost"] = encode_cost(cost);
        body["solution"] = encode_solution(
            *record->input,
            *record->solution);
        return detail::json_response(200, std::move(body));
    }

    [[nodiscard]] auto remove_run(const std::string& id) -> crow::response
    {
        auto record = find_run(id);
        if (!record)
        {
            return detail::error_response(404, "run '" + id + "' does not exist");
        }

        {
            const std::lock_guard lock{record->mutex};
            if (record->state == run_state::queued ||
                record->state == run_state::running)
            {
                if (!record->supports_stop)
                {
                    return detail::error_response(
                        409,
                        "runner does not support cooperative cancellation");
                }

                record->stop_source.request_stop();
                crow::json::wvalue body;
                body["run_id"] = id;
                body["status"] = "cancellation_requested";
                return detail::json_response(202, std::move(body));
            }
        }

        {
            const std::lock_guard lock{runs_mutex_};
            runs_.erase(id);
        }

        crow::json::wvalue body;
        body["run_id"] = id;
        body["removed"] = true;
        return detail::json_response(200, std::move(body));
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

        CROW_BP_ROUTE(blueprint_, "/runs/<string>")
        .methods(crow::HTTPMethod::DELETE)
        ([this](std::string id) {
            return remove_run(id);
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
