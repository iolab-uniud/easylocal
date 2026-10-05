#pragma once

/// \file
/// app: a problem's components gathered under a name, written as a pipe
/// (app("tsp") | solution_manager | neighborhood | runner<...>("name", {...})).
///
/// An app is bound to an Input (bound_app) to get its services, runs any of its
/// runners by name, and exposes all its parameters as one parameter_set. Tools
/// (Session, TextUI, REST) work on apps.

#include <easylocal/runners/runner.hpp>
#include <easylocal/solvers.hpp>
#include <easylocal/utils/detail/attributes.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal
{

/// The effort of a run, when its algorithm reports it: the built-in runners
/// do, through search_result.
struct run_effort
{
    /// Solutions and moves evaluated, the initial evaluation included.
    std::size_t evaluations{};
    /// Iterations, as the algorithm counts them.
    std::size_t iterations{};
    /// Why the run ended.
    termination_reason termination{termination_reason::completed};
};

/// The result of a runner chosen by name.
///
/// Each algorithm has its own result type; what every result provides
/// (search_result_for) is the solution and its cost, and the effort when the
/// result has it.
template<class Solution, class Cost>
struct named_run_result
{
    /// The solution the runner returns.
    Solution solution;
    /// Its cost.
    Cost cost;
    /// The effort of the run; empty when the result does not report it.
    std::optional<run_effort> effort;
};

namespace detail
{

// A runner algorithm registered in an app is identified by its own class and
// is constructed from its default-initializable parameters_type.
template<class Algorithm>
concept configurable_app_algorithm =
    requires {
        typename Algorithm::parameters_type;
    } &&
    std::default_initializable<typename Algorithm::parameters_type> &&
    std::constructible_from<Algorithm, typename Algorithm::parameters_type>;

template<configurable_app_algorithm Algorithm>
struct app_runner_registration
{
    using algorithm_type = Algorithm;
    using config_type = typename Algorithm::parameters_type;

    std::string name;
    config_type config{};
};

// A pipeline registered in an app: run by name from the current solution, its
// stages' parameters under runners.<name>.
template<class Pipeline>
struct app_pipeline_registration
{
    using pipeline_type = Pipeline;

    std::string name;
    Pipeline pipeline;
};

template<class Registration>
inline constexpr bool is_pipeline_registration_v = false;

template<class Pipeline>
inline constexpr bool is_pipeline_registration_v<app_pipeline_registration<Pipeline>> =
    true;

// The algorithm a registration runs on the app's services: a runner's, none for
// a pipeline, whose stages have their own.
struct no_algorithm
{
};

template<class Registration>
struct registration_algorithm
{
    using type = typename Registration::algorithm_type;
};

template<class Pipeline>
struct registration_algorithm<app_pipeline_registration<Pipeline>>
{
    using type = no_algorithm;
};

template<class Registration>
using registration_algorithm_t = typename registration_algorithm<Registration>::type;

template<class Algorithm, class... Registrations>
inline constexpr std::size_t app_runner_count_v =
    (std::size_t{0} + ...
        + (std::same_as<Algorithm, registration_algorithm_t<Registrations>> ? 1U : 0U));

template<class Algorithm, std::size_t Index, class First, class... Rest>
consteval std::size_t app_runner_index_impl()
{
    if constexpr (std::same_as<Algorithm, registration_algorithm_t<First>>)
    {
        return Index;
    }
    else
    {
        static_assert(sizeof...(Rest) != 0, "runner algorithm is not registered in app");
        return app_runner_index_impl<Algorithm, Index + 1, Rest...>();
    }
}

template<class Algorithm, class... Registrations>
consteval std::size_t app_runner_index()
{
    static_assert(
        app_runner_count_v<Algorithm, Registrations...> == 1,
        "runner<Algorithm>() requires exactly one registration of Algorithm");
    return app_runner_index_impl<Algorithm, 0, Registrations...>();
}

// The registration of Algorithm called name, const when registrations is.
template<class Algorithm, std::size_t Index = 0, class Tuple>
[[nodiscard]]
std::conditional_t<
    std::is_const_v<Tuple>,
    const app_runner_registration<Algorithm>,
    app_runner_registration<Algorithm>>&
app_runner_registration_by_name(Tuple& registrations, const std::string_view name)
{
    if constexpr (Index == std::tuple_size_v<std::remove_const_t<Tuple>>)
    {
        throw std::invalid_argument{
            "runner '" + std::string{name} + "' is not registered for the requested algorithm"};
    }
    else
    {
        using registration_type = std::tuple_element_t<Index, std::remove_const_t<Tuple>>;

        if constexpr (std::same_as<
                          Algorithm,
                          registration_algorithm_t<registration_type>>)
        {
            auto& registration = std::get<Index>(registrations);
            if (registration.name == name)
            {
                return registration;
            }
        }

        return app_runner_registration_by_name<Algorithm, Index + 1>(
            registrations,
            name);
    }
}

template<class Algorithm, class SM, class NHE>
class app_runner_ref
{
public:
    using solution_manager_type = SM;
    using neighborhood_explorer_type = NHE;
    using solution_type = typename SM::solution_type;
    using cost_type = typename SM::cost_type;

    app_runner_ref(Algorithm& algorithm, SM& solution_manager, NHE& neighborhood) noexcept
        : algorithm_{algorithm},
          solution_manager_{solution_manager},
          neighborhood_{neighborhood}
    {
    }

    [[nodiscard]]
    const SM& solution_manager() const noexcept
    {
        return solution_manager_;
    }

    [[nodiscard]]
    const NHE& neighborhood_explorer() const noexcept
    {
        return neighborhood_;
    }

    [[nodiscard]]
    solution_type initial_solution() const
        requires has_initial_solution<SM>
    {
        return solution_manager_.initial_solution();
    }

    template<class RNG>
    [[nodiscard]]
    solution_type random_solution(RNG& rng) const
        requires has_random_solution<SM, RNG>
    {
        return solution_manager_.random_solution(rng);
    }

    template<class... RunArgs>
    [[nodiscard]]
    auto run(solution_type solution, RunArgs&&... run_args)
        requires algorithm_runnable<Algorithm, runner_context<SM, NHE>, RunArgs...>
    {
        assert(
            solution_manager_.is_valid(solution) &&
            "initial Solution must be compatible with the app Input");

        const runner_context<SM, NHE> context{
            solution_manager_,
            neighborhood_,
        };

        return run_algorithm(
            algorithm_,
            context,
            std::move(solution),
            std::forward<RunArgs>(run_args)...);
    }

    // Whether the algorithm takes an RNG as its run argument.
    template<class RNG, class... Options>
    static constexpr bool takes_rng =
        algorithm_runnable<Algorithm, runner_context<SM, NHE>, RNG&, Options...>;

    // The run used by tools: the tool's RNG is passed to stochastic algorithms
    // and ignored by deterministic ones; options are with(control, tracer).
    template<std::uniform_random_bit_generator RNG, class... Options>
    [[nodiscard]]
    auto run_with_rng(solution_type solution, RNG& rng, Options&&... options)
        requires takes_rng<RNG, Options...> ||
                 algorithm_runnable<Algorithm, runner_context<SM, NHE>, Options...>
    {
        if constexpr (takes_rng<RNG, Options...>)
        {
            return run(std::move(solution), rng, std::forward<Options>(options)...);
        }
        else
        {
            return run(std::move(solution), std::forward<Options>(options)...);
        }
    }

private:
    Algorithm& algorithm_;
    SM& solution_manager_;
    NHE& neighborhood_;
};

template<class SMSpec, class NHESpec, class... Registrations>
    requires is_solution_manager_spec_v<SMSpec> && is_neighborhood_spec_v<NHESpec>
    && detail::evaluable_solution_manager<service_t<SMSpec>>
    && easylocal::neighborhood_explorer_for<service_t<NHESpec>, service_t<SMSpec>>
class bound_app
{
public:
    using solution_manager_type = service_t<SMSpec>;
    using neighborhood_explorer_type = service_t<NHESpec>;
    using input_type = typename solution_manager_type::input_type;

    static constexpr std::size_t runner_count = sizeof...(Registrations);

    bound_app(
        const input_type& input,
        const SMSpec& solution_manager_spec,
        const NHESpec& neighborhood_spec,
        const std::tuple<Registrations...>& registrations)
        : input_{input},
          solution_manager_{solution_manager_spec.construct(input_)},
          neighborhood_{neighborhood_spec.construct(solution_manager_)},
          algorithms_{make_algorithms(registrations)}
    {
        assert(
            std::addressof(solution_manager_.input()) ==
                std::addressof(input_));
        assert(
            std::addressof(neighborhood_.input()) ==
                std::addressof(input_));
    }

    bound_app(const bound_app&) = delete;
    bound_app& operator=(const bound_app&) = delete;
    bound_app(bound_app&&) = delete;
    bound_app& operator=(bound_app&&) = delete;

    [[nodiscard]]
    const input_type& input() const noexcept
    {
        return input_;
    }

    template<class Self>
    [[nodiscard]]
    auto& solution_manager(this Self&& self) noexcept
    {
        return self.solution_manager_;
    }

    template<class Self>
    [[nodiscard]]
    auto& neighborhood(this Self&& self) noexcept
    {
        return self.neighborhood_;
    }

    template<class Algorithm>
    [[nodiscard]]
    auto runner()
    {
        constexpr auto index = app_runner_index<Algorithm, Registrations...>();
        using registration_type =
            std::tuple_element_t<index, std::tuple<Registrations...>>;
        using algorithm_type = typename registration_type::algorithm_type;

        return app_runner_ref<
            algorithm_type,
            solution_manager_type,
            neighborhood_explorer_type>{
            std::get<index>(algorithms_),
            solution_manager_,
            neighborhood_,
        };
    }

    template<std::size_t Index>
        requires (Index < runner_count)
    [[nodiscard]]
    auto runner_at()
    {
        using registration_type =
            std::tuple_element_t<Index, std::tuple<Registrations...>>;
        using algorithm_type = typename registration_type::algorithm_type;

        return app_runner_ref<
            algorithm_type,
            solution_manager_type,
            neighborhood_explorer_type>{
            std::get<Index>(algorithms_),
            solution_manager_,
            neighborhood_,
        };
    }

    template<class Algorithm, class... RunArgs>
    [[nodiscard]]
    auto run(typename solution_manager_type::solution_type solution, RunArgs&&... args)
    {
        return runner<Algorithm>().run(
            std::move(solution),
            std::forward<RunArgs>(args)...);
    }

    template<std::size_t Index, class... RunArgs>
        requires (Index < runner_count)
    [[nodiscard]]
    auto run_at(
        typename solution_manager_type::solution_type solution,
        RunArgs&&... args)
    {
        return runner_at<Index>().run(
            std::move(solution),
            std::forward<RunArgs>(args)...);
    }

    template<std::size_t Index, std::uniform_random_bit_generator RNG, class... Options>
        requires (Index < runner_count)
    [[nodiscard]]
    auto run_at_with_rng(
        typename solution_manager_type::solution_type solution,
        RNG& rng,
        Options&&... options)
    {
        return runner_at<Index>().run_with_rng(
            std::move(solution),
            rng,
            std::forward<Options>(options)...);
    }

private:
    static auto make_algorithms(const std::tuple<Registrations...>& registrations)
    {
        return std::apply(
            [](const auto&... registration) {
                return std::tuple<registration_algorithm_t<Registrations>...>{
                    make_algorithm(registration)...};
            },
            registrations);
    }

    template<class Registration>
    static registration_algorithm_t<Registration> make_algorithm(
        const Registration& registration)
    {
        if constexpr (is_pipeline_registration_v<Registration>)
            return {};
        else
            return registration_algorithm_t<Registration>{registration.config};
    }

    const input_type& input_;
    solution_manager_type solution_manager_;
    neighborhood_explorer_type neighborhood_;
    std::tuple<registration_algorithm_t<Registrations>...> algorithms_;
};

template<class Spec>
struct app_input_type
{
};

template<class Spec>
    requires is_solution_manager_spec_v<Spec>
struct app_input_type<Spec>
{
    using input_type = typename service_t<Spec>::input_type;
};

template<class SMSpec, class NHESpec, class... Registrations>
class app_builder : public app_input_type<SMSpec>
{
    static_assert(
        std::copy_constructible<SMSpec> &&
        std::copy_constructible<NHESpec> &&
        (std::copy_constructible<Registrations> && ...),
        "app graph specifications and runner registrations must be copy constructible");

public:
    // Whether the app, with Spec as its solution manager recipe, can be bound
    // and run: a solution manager, a neighborhood and a runner at least.
    template<class Spec>
    static constexpr bool complete = !std::same_as<Spec, unconfigured_t>
        && !std::same_as<NHESpec, unconfigured_t> && (sizeof...(Registrations) > 0);

    static constexpr bool has_solution_manager =
        !std::same_as<SMSpec, unconfigured_t>;
    static constexpr bool has_neighborhood =
        !std::same_as<NHESpec, unconfigured_t>;
    static constexpr std::size_t runner_count = sizeof...(Registrations);

    explicit app_builder(std::string name)
        : name_{std::move(name)}
    {
    }

    app_builder(
        std::string name,
        SMSpec solution_manager_spec,
        NHESpec neighborhood_spec,
        std::tuple<Registrations...> registrations)
        : name_{std::move(name)},
          solution_manager_spec_{std::move(solution_manager_spec)},
          neighborhood_spec_{std::move(neighborhood_spec)},
          registrations_{std::move(registrations)}
    {
    }

    [[nodiscard]]
    std::string_view name() const noexcept
    {
        return name_;
    }

    template<class Spec>
        requires std::same_as<SMSpec, unconfigured_t> &&
                 is_solution_manager_spec_v<std::remove_cvref_t<Spec>>
    [[nodiscard]]
    auto with_solution_manager(Spec&& spec) &&
    {
        using spec_type = std::remove_cvref_t<Spec>;
        static_assert(validate_solution_manager_spec<spec_type>());
        return app_builder<spec_type, NHESpec, Registrations...>{
            std::move(name_),
            std::forward<Spec>(spec),
            std::move(neighborhood_spec_),
            std::move(registrations_),
        };
    }

    template<class Spec>
        requires (!std::same_as<SMSpec, unconfigured_t>) &&
                 std::same_as<NHESpec, unconfigured_t> &&
                 is_neighborhood_spec_v<std::remove_cvref_t<Spec>>
    [[nodiscard]]
    auto with_neighborhood(Spec&& spec) &&
    {
        using spec_type = std::remove_cvref_t<Spec>;
        return app_builder<SMSpec, spec_type, Registrations...>{
            std::move(name_),
            std::move(solution_manager_spec_),
            std::forward<Spec>(spec),
            std::move(registrations_),
        };
    }

    template<configurable_app_algorithm Algorithm>
        requires (!std::same_as<SMSpec, unconfigured_t>) &&
                 (!std::same_as<NHESpec, unconfigured_t>)
    [[nodiscard]]
    auto with_runner(
        std::string name,
        typename Algorithm::parameters_type parameters = {}) &&
    {
        return std::move(*this).with_runner(app_runner_registration<Algorithm>{
            .name = std::move(name),
            .config = std::move(parameters),
        });
    }

    template<configurable_app_algorithm Algorithm>
        requires (!std::same_as<SMSpec, unconfigured_t>) &&
                 (!std::same_as<NHESpec, unconfigured_t>)
    [[nodiscard]]
    auto with_runner(app_runner_registration<Algorithm> registration) &&
    {
        using registration_type = app_runner_registration<Algorithm>;
        auto registrations = std::tuple_cat(
            std::move(registrations_),
            std::tuple<registration_type>{std::move(registration)});

        return app_builder<
            SMSpec,
            NHESpec,
            Registrations...,
            registration_type>{
            std::move(name_),
            std::move(solution_manager_spec_),
            std::move(neighborhood_spec_),
            std::move(registrations),
        };
    }

    template<class Pipeline>
        requires(!std::same_as<SMSpec, unconfigured_t>)
        && (!std::same_as<NHESpec, unconfigured_t>)
    [[nodiscard]]
    auto with_pipeline(app_pipeline_registration<Pipeline> registration) &&
    {
        using solution_manager_type = service_t<SMSpec>;
        static_assert(
            std::same_as<
                typename Pipeline::input_type,
                typename solution_manager_type::input_type>
                && std::same_as<
                    typename Pipeline::solution_type,
                    typename solution_manager_type::solution_type>,
            "a pipeline registered in an app must have the app's Input and Solution");
        static_assert(
            std::same_as<
                typename Pipeline::cost_type,
                typename solution_manager_type::cost_type>,
            "the last stage of a pipeline registered in an app must have the app's "
            "cost");
        using registration_type = app_pipeline_registration<Pipeline>;
        auto registrations = std::tuple_cat(
            std::move(registrations_),
            std::tuple<registration_type>{std::move(registration)});

        return app_builder<SMSpec, NHESpec, Registrations..., registration_type>{
            std::move(name_),
            std::move(solution_manager_spec_),
            std::move(neighborhood_spec_),
            std::move(registrations),
        };
    }

    // The parameters of the only runner of type Algorithm, const when the app
    // is.
    template<class Algorithm, class Self>
        requires(app_runner_count_v<Algorithm, Registrations...> == 1)
    [[nodiscard]]
    auto& runner_config(this Self&& self) noexcept
    {
        constexpr auto index = app_runner_index<Algorithm, Registrations...>();
        return std::get<index>(self.registrations_).config;
    }

    // The parameters of the runner of type Algorithm registered as name,
    // const when the app is.
    template<class Algorithm, class Self>
        requires(app_runner_count_v<Algorithm, Registrations...> > 0)
    [[nodiscard]]
    auto& runner_config(this Self&& self, const std::string_view name)
    {
        return app_runner_registration_by_name<Algorithm>(self.registrations_, name)
            .config;
    }

    template<class Algorithm>
        requires(app_runner_count_v<Algorithm, Registrations...> == 1)
    [[nodiscard]]
    std::string_view runner_name() const noexcept
    {
        constexpr auto index = app_runner_index<Algorithm, Registrations...>();
        return std::get<index>(registrations_).name;
    }

    // The parameters of the app: its cost expression ("cost"), its
    // neighborhood ("neighborhood") and each registered runner's
    // ("runners.<name>", for parameters that are a parameter block). Every
    // run reads them, so a change applies from the next run; read-only when
    // the app is const. The set refers to this app, which must stay in place
    // while it is used: a temporary app has no configuration().
    template<class Self>
    [[nodiscard]]
    config::parameter_set configuration(this Self& self)
    {
        config::parameter_set parameters;
        config::add_configuration(parameters, "cost", self.solution_manager_spec_);
        config::add_configuration(parameters, "neighborhood", self.neighborhood_spec_);
        std::apply(
            [&](auto&... registration) {
                (add_runner_configuration(parameters, registration), ...);
            },
            self.registrations_);
        return parameters;
    }

    // Visits the runners (not the pipelines), in the order they were
    // registered: visitor.template operator()<Algorithm>(name, config).
    template<class Visitor>
    void for_each_runner_registration(Visitor&& visitor) const
    {
        auto by_type =
            [&]<class Algorithm, std::size_t>(
                const std::string_view name,
                const typename Algorithm::parameters_type& config) {
                visitor.template operator()<Algorithm>(name, config);
            };
        [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            (visit_runner<Index>(by_type), ...);
        }(std::index_sequence_for<Registrations...>{});
    }

    // Visits the name of every registration, runner or pipeline, in order: the
    // names the tools list and run.
    template<class Visitor>
    void for_each_registration_name(Visitor&& visitor) const
    {
        std::apply(
            [&](const auto&... registration) {
                (visitor(std::string_view{registration.name}), ...);
            },
            registrations_);
    }

    template<class Algorithm>
        requires (app_runner_count_v<Algorithm, Registrations...> == 1) &&
                 std::copy_constructible<SMSpec> &&
                 std::copy_constructible<NHESpec>
    [[nodiscard]]
    auto make_runner() const
    {
        constexpr auto index = app_runner_index<Algorithm, Registrations...>();
        const auto& registration = std::get<index>(registrations_);
        return easylocal::make_runner<Algorithm>(registration.config)
            | solution_manager_spec_ | neighborhood_spec_;
    }

    template<class Algorithm>
        requires (app_runner_count_v<Algorithm, Registrations...> > 0) &&
                 std::copy_constructible<SMSpec> &&
                 std::copy_constructible<NHESpec>
    [[nodiscard]]
    auto make_runner(const std::string_view name) const
    {
        const auto& registration =
            app_runner_registration_by_name<Algorithm>(registrations_, name);
        return easylocal::make_runner<Algorithm>(registration.config)
            | solution_manager_spec_ | neighborhood_spec_;
    }

    template<template<class...> class Solver, class RunnerAlgorithm, class SolverConfig>
        requires (app_runner_count_v<RunnerAlgorithm, Registrations...> == 1) &&
                 std::copy_constructible<SMSpec> &&
                 std::copy_constructible<NHESpec> &&
                 requires {
                     Solver{
                         std::declval<const app_builder&>().template make_runner<RunnerAlgorithm>(),
                         std::declval<SolverConfig>()};
                 }
    [[nodiscard]]
    auto make_solver(SolverConfig&& config) const
    {
        return easylocal::make_solver<Solver>(
            make_runner<RunnerAlgorithm>(),
            std::forward<SolverConfig>(config));
    }

    template<template<class...> class Solver, class RunnerAlgorithm, class SolverConfig>
        requires (app_runner_count_v<RunnerAlgorithm, Registrations...> > 0) &&
                 std::copy_constructible<SMSpec> &&
                 std::copy_constructible<NHESpec> &&
                 requires {
                     Solver{
                         std::declval<const app_builder&>().template make_runner<RunnerAlgorithm>(std::declval<std::string_view>()),
                         std::declval<SolverConfig>()};
                 }
    [[nodiscard]]
    auto make_solver(
        const std::string_view runner_name,
        SolverConfig&& config) const
    {
        return easylocal::make_solver<Solver>(
            make_runner<RunnerAlgorithm>(runner_name),
            std::forward<SolverConfig>(config));
    }

    template<class Spec = SMSpec>
        requires complete<Spec> && Spec::template
    constructible_from<const typename service_t<Spec>::input_type>&& NHESpec::
        template constructible_from<service_t<Spec>> [[nodiscard]]
        auto bind(const typename service_t<Spec>::input_type& input) const
    {
        return bound_app<SMSpec, NHESpec, Registrations...>{
            input,
            solution_manager_spec_,
            neighborhood_spec_,
            registrations_,
        };
    }

    // A bound app stores a reference to its Input. Reject temporaries at the
    // boundary instead of permitting a bound app with a dangling reference.
    template<class Spec = SMSpec>
        requires complete<Spec>
    auto bind(typename service_t<Spec>::input_type&&) const = delete;

    template<class Spec = SMSpec>
        requires complete<Spec>
    auto bind(const typename service_t<Spec>::input_type&&) const = delete;

    // Execute one runner against a freshly bound app.  The app graph and
    // immutable Input may be shared across concurrent calls; mutable
    // SolutionManager, Neighborhood and algorithm state are reconstructed for
    // every invocation, from the current runner parameters.  Adapters can
    // therefore schedule independent runs without making bound_app itself
    // thread-safe.
    template<class Algorithm, class Spec = SMSpec, class... RunArgs>
        requires complete<Spec>
    [[nodiscard]]
    auto run(
        const typename service_t<Spec>::input_type& input,
        typename service_t<Spec>::solution_type solution,
        RunArgs&&... args) const
    {
        auto bound = bind(input);
        return bound.template run<Algorithm>(
            std::move(solution),
            std::forward<RunArgs>(args)...);
    }

    template<std::size_t Index, class Spec = SMSpec, class... RunArgs>
        requires complete<Spec> && (Index < sizeof...(Registrations))
    [[nodiscard]]
    auto run_at(
        const typename service_t<Spec>::input_type& input,
        typename service_t<Spec>::solution_type solution,
        RunArgs&&... args) const
    {
        auto bound = bind(input);
        return bound.template run_at<Index>(
            std::move(solution),
            std::forward<RunArgs>(args)...);
    }

    // Runs registration Index on a freshly bound app, giving rng to the
    // algorithm if it takes one.
    template<
        std::size_t Index,
        std::uniform_random_bit_generator RNG,
        class Spec = SMSpec,
        class... Options>
        requires complete<Spec> && (Index < sizeof...(Registrations))
    [[nodiscard]]
    auto run_at_with_rng(
        const typename service_t<Spec>::input_type& input,
        typename service_t<Spec>::solution_type solution,
        RNG& rng,
        Options&&... options) const
    {
        auto bound = bind(input);
        return bound.template run_at_with_rng<Index>(
            std::move(solution),
            rng,
            std::forward<Options>(options)...);
    }

    // Runs the runner or the pipeline registered under name from solution: a
    // runner on a freshly bound app, giving rng to the algorithm if it takes
    // one, a pipeline with its stages' own recipes and rng; options are
    // with(control, tracer). Empty when nothing has that name. This is how tools
    // run the runner a user picks.
    template<std::uniform_random_bit_generator RNG, class Spec = SMSpec, class... Options>
        requires complete<Spec>
    [[nodiscard]]
    auto run(
        const std::string_view name,
        const typename service_t<Spec>::input_type& input,
        typename service_t<Spec>::solution_type solution,
        RNG& rng,
        Options&&... options) const
    {
        using solution_type = typename service_t<Spec>::solution_type;
        using cost_type = typename service_t<Spec>::cost_type;

        std::optional<named_run_result<solution_type, cost_type>> outcome;
        const auto run_registration =
            [&]<std::size_t Index>() {
                const auto& registration = std::get<Index>(registrations_);
                if (outcome || registration.name != name)
                    return;

                auto result = [&] {
                    if constexpr (is_pipeline_registration_v<
                                      std::remove_cvref_t<decltype(registration)>>)
                        return registration.pipeline
                            .run(input, solution, rng, options...);
                    else
                        return run_at_with_rng<Index>(
                            input,
                            std::move(solution),
                            rng,
                            std::forward<Options>(options)...);
                }();
                static_assert(
                    search_result_for<decltype(result), solution_type, cost_type>,
                    "running a runner by name requires its result to provide the "
                    "solution and its cost (see easylocal::search_result_for)");
                std::optional<run_effort> effort;
                if constexpr (requires {
                                  {
                                      result.evaluations
                                  } -> std::convertible_to<std::size_t>;
                                  {
                                      result.iterations
                                  } -> std::convertible_to<std::size_t>;
                                  {
                                      result.termination
                                  } -> std::convertible_to<termination_reason>;
                              })
                {
                    effort = run_effort{
                        .evaluations = result.evaluations,
                        .iterations = result.iterations,
                        .termination = result.termination,
                    };
                }
                outcome.emplace(
                    named_run_result<solution_type, cost_type>{
                        .solution = std::move(result.solution),
                        .cost = result.cost,
                        .effort = effort,
                    });
            };
        [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            (run_registration.template operator()<Index>(), ...);
        }(std::index_sequence_for<Registrations...>{});
        return outcome;
    }

    /// Adds a SolutionManager to the app: the pipe spelling of
    /// with_solution_manager.
    template<class Spec>
        requires requires(app_builder builder, Spec&& spec) {
            std::move(builder).with_solution_manager(std::forward<Spec>(spec));
        }
    [[nodiscard]]
    friend auto operator|(app_builder builder, Spec&& spec)
    {
        return std::move(builder).with_solution_manager(std::forward<Spec>(spec));
    }

    /// Adds a neighborhood to the app: the pipe spelling of with_neighborhood.
    template<class Spec>
        requires requires(app_builder builder, Spec&& spec) {
            std::move(builder).with_neighborhood(std::forward<Spec>(spec));
        }
    [[nodiscard]]
    friend auto operator|(app_builder builder, Spec&& spec)
    {
        return std::move(builder).with_neighborhood(std::forward<Spec>(spec));
    }

    /// Registers a runner in the app: the pipe spelling of with_runner.
    template<class Algorithm>
        requires requires(
            app_builder builder,
            app_runner_registration<Algorithm> registration) {
            std::move(builder).with_runner(std::move(registration));
        }
    [[nodiscard]]
    friend auto operator|(
        app_builder builder,
        app_runner_registration<Algorithm> registration)
    {
        return std::move(builder).with_runner(std::move(registration));
    }

    /// Registers a pipeline in the app: the pipe spelling of with_pipeline.
    template<class Pipeline>
    [[nodiscard]]
    friend auto operator|(
        app_builder builder,
        app_pipeline_registration<Pipeline> registration)
    {
        return std::move(builder).with_pipeline(std::move(registration));
    }

private:
    template<class Registration>
    static void add_runner_configuration(
        config::parameter_set& parameters,
        Registration& registration)
    {
        if constexpr (is_pipeline_registration_v<std::remove_const_t<Registration>>)
        {
            parameters.add(
                "runners." + registration.name,
                registration.pipeline.configuration());
        }
        else
        {
            using parameters_type =
                typename std::remove_const_t<Registration>::config_type;
            if constexpr (config::parameter_block<parameters_type>)
                parameters.add("runners." + registration.name, registration.config);
        }
    }

    template<std::size_t Index, class Visitor>
    void visit_runner(Visitor& visitor) const
    {
        using registration_type =
            std::tuple_element_t<Index, std::tuple<Registrations...>>;
        if constexpr (!is_pipeline_registration_v<registration_type>)
        {
            const auto& registration = std::get<Index>(registrations_);
            visitor.template
            operator()<typename registration_type::algorithm_type, Index>(
                std::string_view{registration.name},
                registration.config);
        }
    }

    std::string name_;
    EASYLOCAL_NO_UNIQUE_ADDRESS SMSpec solution_manager_spec_{};
    EASYLOCAL_NO_UNIQUE_ADDRESS NHESpec neighborhood_spec_{};
    std::tuple<Registrations...> registrations_{};
};

} // namespace detail

/// Starts an app named name, without components: a SolutionManager, a
/// neighborhood and runners are added to it with `|` (or with_solution_manager,
/// with_neighborhood and with_runner).
[[nodiscard]]
inline auto app(std::string name)
{
    return detail::app_builder<
        detail::unconfigured_t,
        detail::unconfigured_t>{std::move(name)};
}

/// A named runner registration for an app, e.g.
/// app("tsp") | sm | nhe | runner<runners::FirstImprovement>("fi", {...}).
template<detail::configurable_app_algorithm Algorithm>
[[nodiscard]]
detail::app_runner_registration<Algorithm> runner(
    std::string name,
    typename Algorithm::parameters_type parameters = {})
{
    return {.name = std::move(name), .config = std::move(parameters)};
}

/// A named registration of a pipeline already built, e.g.
/// pipeline("cascade", (stage(...) & until_feasible()) | stage(...)).
template<std::uniform_random_bit_generator RNG, class... Stages>
[[nodiscard]]
detail::app_pipeline_registration<solvers::Pipeline<RNG, Stages...>> pipeline(
    std::string name,
    solvers::Pipeline<RNG, Stages...> pipeline)
{
    return {.name = std::move(name), .pipeline = std::move(pipeline)};
}

/// A named pipeline registration for an app, run by name from the current
/// solution like a runner, e.g.
/// app("tsp") | sm | nhe | pipeline("cascade", stage(...), stage(...)).
///
/// Its parameters are its stages' under `runners.<name>`. Requires stages with
/// the app's Input and Solution, the last one with the app's cost.
template<class... Stages>
    requires(sizeof...(Stages) > 0)
    && (solvers::detail::is_pipeline_stage_v<Stages> && ...)
[[nodiscard]]
auto pipeline(std::string name, Stages... stages)
{
    return easylocal::pipeline(std::move(name), solvers::pipeline(std::move(stages)...));
}

} // namespace easylocal
