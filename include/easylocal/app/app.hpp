#pragma once

/// \file
/// App: a problem's components gathered under a name, written as a pipe
/// (app("tsp") | solution_manager | neighborhood | runner<...>("name", {...})).
///
/// An app is bound to an Input (BoundApp) to get its services, runs any of its
/// runners by name, and exposes all its parameters as one parameter_set. Tools
/// (Session, TextUI, REST) work on apps.

#include <easylocal/runners/runner.hpp>
#include <easylocal/solvers.hpp>
#include <easylocal/utils/detail/attributes.hpp>

#include <algorithm>
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
#include <vector>

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

// A runner registered in an app: its name, its parameters and, unless it runs
// on the app's neighborhood (unconfigured_t), the recipe of its own.
template<configurable_app_algorithm Algorithm, class NeighborhoodSpec = unconfigured_t>
struct app_runner_registration
{
    using algorithm_type = Algorithm;
    using config_type = typename Algorithm::parameters_type;
    using neighborhood_spec_type = NeighborhoodSpec;

    std::string name;
    config_type config{};
    EASYLOCAL_NO_UNIQUE_ADDRESS NeighborhoodSpec neighborhood;
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

// Whether a registration brings its own neighborhood recipe.
template<class Registration>
inline constexpr bool has_own_neighborhood_v = false;

template<class Algorithm, class NeighborhoodSpec>
inline constexpr bool
    has_own_neighborhood_v<app_runner_registration<Algorithm, NeighborhoodSpec>> =
        !std::same_as<NeighborhoodSpec, unconfigured_t>;

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
        "this lookup by algorithm requires exactly one registration of Algorithm");
    return app_runner_index_impl<Algorithm, 0, Registrations...>();
}

// The index of the only registration of Algorithm in a tuple of registrations.
template<class Algorithm, class Tuple>
struct app_runner_index_in;

template<class Algorithm, class... Registrations>
struct app_runner_index_in<Algorithm, std::tuple<Registrations...>>
{
    static constexpr std::size_t value = app_runner_index<Algorithm, Registrations...>();
};

// Calls visit(registration) on the registration of Algorithm called name and
// returns what it returns, a Result; throws std::invalid_argument when there
// is none.
template<class Result, class Algorithm, std::size_t Index = 0, class Tuple, class Visit>
Result visit_runner_registration_named(
    Tuple& registrations,
    const std::string_view name,
    Visit&& visit)
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
                return visit(registration);
        }

        return visit_runner_registration_named<Result, Algorithm, Index + 1>(
            registrations,
            name,
            visit);
    }
}

// Whether Algorithm runs on the services of SM and NHE as a tool runs it: with
// the tool's RNG, or without one.
template<class Algorithm, class SM, class NHE>
inline constexpr bool app_runnable_v =
    algorithm_runnable<Algorithm, runner_context<SM, NHE>>
    || algorithm_runnable<Algorithm, runner_context<SM, NHE>, std::mt19937_64&>;

// The checks of a runner registration with its own neighborhood, when it is
// added to an app: the neighborhood explores the app's solutions and the
// algorithm runs on it.
template<class Algorithm, class SM, class NHESpec>
consteval bool validate_app_runner()
{
    using neighborhood_type = service_t<NHESpec>;
    if constexpr (!runner_neighborhood_explorer<neighborhood_type, SM>)
    {
        static_assert(
            runner_neighborhood_explorer<neighborhood_type, SM>,
            "the neighborhood of a runner registration must explore the "
            "Solution of the app's SolutionManager");
        return false;
    }
    else
    {
        static_assert(
            app_runnable_v<Algorithm, SM, neighborhood_type>,
            "a registered runner's algorithm must run on its neighborhood, with "
            "or without an RNG: run(run, solution[, rng])");
        return validate_delta_bindings<SM, neighborhood_type>();
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

// What a bound app holds for a registration's own neighborhood: nothing when it
// runs on the app's, else the explorer built over the app's SolutionManager.
template<class Registration, class SM>
struct own_neighborhood_source
{
    const Registration& registration;
    SM& solution_manager;
};

template<class Registration, class SM>
struct own_neighborhood_slot
{
    explicit own_neighborhood_slot(
        const own_neighborhood_source<Registration, SM>&) noexcept
    {
    }
};

template<class Registration, class SM>
    requires has_own_neighborhood_v<Registration>
struct own_neighborhood_slot<Registration, SM>
{
    explicit own_neighborhood_slot(
        const own_neighborhood_source<Registration, SM>& source)
        : value{source.registration.neighborhood.construct(source.solution_manager)}
    {
    }

    service_t<typename Registration::neighborhood_spec_type> value;
};

// The members of App and BoundApp keyed by algorithm type rather than by name,
// for the library and its tests: a type registered twice has no single runner.
struct app_access;

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

} // namespace detail

/// An app bound to an Input: the services built for it once (the
/// SolutionManager, the app's neighborhood explorer, those of the runners that
/// have their own) and the runners, run by name on them.
///
/// App::bind returns it; it borrows the Input, which must outlive it, and
/// cannot be copied or moved, since its services refer to each other. The
/// runners keep their state from one run to the next, and their parameters as
/// they were at bind. Requires a SolutionManager recipe with a cost and a
/// neighborhood recipe for it.
template<class SMSpec, class NHESpec, class... Registrations>
    requires detail::is_solution_manager_spec_v<SMSpec>
    && detail::is_neighborhood_spec_v<NHESpec>
    && detail::evaluable_solution_manager<detail::service_t<SMSpec>>
    && easylocal::neighborhood_explorer_for<
        detail::service_t<NHESpec>,
        detail::service_t<SMSpec>>
class BoundApp
{
public:
    /// The SolutionManager built from the recipe.
    using solution_manager_type = detail::service_t<SMSpec>;
    /// The app's neighborhood explorer built from the recipe.
    using neighborhood_explorer_type = detail::service_t<NHESpec>;
    /// The Input of the problem.
    using input_type = typename solution_manager_type::input_type;
    /// The Solution of the problem.
    using solution_type = typename solution_manager_type::solution_type;
    /// The cost of a solution.
    using cost_type = typename solution_manager_type::cost_type;

    /// The services of the app's recipes built for input, and its runners
    /// constructed from their parameters.
    BoundApp(
        const input_type& input,
        const SMSpec& solution_manager_spec,
        const NHESpec& neighborhood_spec,
        const std::tuple<Registrations...>& registrations)
        : BoundApp{
              input,
              solution_manager_spec,
              neighborhood_spec,
              registrations,
              std::index_sequence_for<Registrations...>{}}
    {
    }

    BoundApp(const BoundApp&) = delete;
    BoundApp& operator=(const BoundApp&) = delete;
    BoundApp(BoundApp&&) = delete;
    BoundApp& operator=(BoundApp&&) = delete;

    /// The Input the app is bound to.
    [[nodiscard]]
    const input_type& input() const noexcept
    {
        return input_;
    }

    /// The SolutionManager, const when the bound app is.
    template<class Self>
    [[nodiscard]]
    auto& solution_manager(this Self&& self) noexcept
    {
        return self.solution_manager_;
    }

    /// The app's neighborhood explorer, const when the bound app is.
    template<class Self>
    [[nodiscard]]
    auto& neighborhood(this Self&& self) noexcept
    {
        return self.neighborhood_;
    }

    /// Runs the runner or the pipeline registered under name from solution and
    /// returns its result; empty when nothing has that name.
    ///
    /// A runner runs on these services, with its own neighborhood when it has
    /// one, and gets rng when its algorithm takes one; a pipeline runs with its
    /// stages' own recipes and rng. The options are run options, such as
    /// with(control, tracer). The solution must be valid for the Input.
    template<std::uniform_random_bit_generator RNG, class... Options>
    [[nodiscard]]
    std::optional<named_run_result<solution_type, cost_type>> run(
        const std::string_view name,
        solution_type solution,
        RNG& rng,
        Options&&... options)
    {
        std::optional<named_run_result<solution_type, cost_type>> outcome;
        const auto run_registration = [&]<std::size_t Index>() {
            const auto& registration = std::get<Index>(registrations_);
            if (outcome || registration.name != name)
                return;

            auto result = [&] {
                if constexpr (detail::is_pipeline_registration_v<
                                  std::remove_cvref_t<decltype(registration)>>)
                    return registration.pipeline.run(input_, solution, rng, options...);
                else
                    return runner_at<Index>().run_with_rng(
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
                              { result.evaluations } -> std::convertible_to<std::size_t>;
                              { result.iterations } -> std::convertible_to<std::size_t>;
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

private:
    friend struct detail::app_access;

    template<std::size_t... Index>
    BoundApp(
        const input_type& input,
        const SMSpec& solution_manager_spec,
        const NHESpec& neighborhood_spec,
        const std::tuple<Registrations...>& registrations,
        std::index_sequence<Index...>)
        : input_{input},
          solution_manager_{solution_manager_spec.construct(input_)},
          neighborhood_{neighborhood_spec.construct(solution_manager_)},
          own_neighborhoods_{
              detail::own_neighborhood_source<Registrations, solution_manager_type>{
                  std::get<Index>(registrations),
                  solution_manager_}...},
          algorithms_{make_algorithm(std::get<Index>(registrations))...},
          registrations_{registrations}
    {
        assert(std::addressof(solution_manager_.input()) == std::addressof(input_));
        assert(std::addressof(neighborhood_.input()) == std::addressof(input_));
    }

    template<std::size_t Index>
    [[nodiscard]]
    auto runner_at()
    {
        using registration_type =
            std::tuple_element_t<Index, std::tuple<Registrations...>>;
        using algorithm_type = typename registration_type::algorithm_type;
        auto& neighborhood = neighborhood_at<Index>();
        return detail::app_runner_ref<
            algorithm_type,
            solution_manager_type,
            std::remove_reference_t<decltype(neighborhood)>>{
            std::get<Index>(algorithms_),
            solution_manager_,
            neighborhood,
        };
    }

    // The neighborhood registration Index runs on: its own, or the app's.
    template<std::size_t Index, class Self>
    [[nodiscard]]
    auto& neighborhood_at(this Self& self) noexcept
    {
        using registration_type =
            std::tuple_element_t<Index, std::tuple<Registrations...>>;
        if constexpr (detail::has_own_neighborhood_v<registration_type>)
            return std::get<Index>(self.own_neighborhoods_).value;
        else
            return self.neighborhood_;
    }

    template<class Registration>
    static detail::registration_algorithm_t<Registration> make_algorithm(
        const Registration& registration)
    {
        if constexpr (detail::is_pipeline_registration_v<Registration>)
            return {};
        else
            return detail::registration_algorithm_t<Registration>{registration.config};
    }

    const input_type& input_;
    solution_manager_type solution_manager_;
    neighborhood_explorer_type neighborhood_;
    std::tuple<detail::own_neighborhood_slot<Registrations, solution_manager_type>...>
        own_neighborhoods_;
    std::tuple<detail::registration_algorithm_t<Registrations>...> algorithms_;
    std::tuple<Registrations...> registrations_;
};

/// A problem's components under a name: a SolutionManager recipe (with its
/// cost), a neighborhood recipe and the runners and pipelines registered by
/// name, which tools list, configure and run.
///
/// easylocal::app(name) starts one, and `|` adds the components, each step a
/// new App type; bind(input) builds its services for an Input (a BoundApp),
/// run(name, input, solution, rng) runs a runner on fresh ones. The name of a
/// registration is its key: the runners and their parameters
/// (`runners.<name>.*`) are found by it.
template<class SMSpec, class NHESpec, class... Registrations>
class App : public detail::app_input_type<SMSpec>
{
    static_assert(
        std::copy_constructible<SMSpec> &&
        std::copy_constructible<NHESpec> &&
        (std::copy_constructible<Registrations> && ...),
        "app graph specifications and runner registrations must be copy constructible");

public:
    /// Whether the app, with Spec as its SolutionManager recipe, can be bound
    /// and run: a SolutionManager, a neighborhood and a runner at least.
    template<class Spec>
    static constexpr bool complete = !std::same_as<Spec, detail::unconfigured_t>
        && !std::same_as<NHESpec, detail::unconfigured_t>
        && (sizeof...(Registrations) > 0);

    /// Whether the app has its SolutionManager recipe.
    static constexpr bool has_solution_manager =
        !std::same_as<SMSpec, detail::unconfigured_t>;
    /// Whether the app has its neighborhood recipe.
    static constexpr bool has_neighborhood =
        !std::same_as<NHESpec, detail::unconfigured_t>;
    /// The number of runners and pipelines registered.
    static constexpr std::size_t runner_count = sizeof...(Registrations);

    /// An app named name, without components.
    explicit App(std::string name) : name_{std::move(name)} {}

    /// An app named name, with these components.
    App(std::string name,
        SMSpec solution_manager_spec,
        NHESpec neighborhood_spec,
        std::tuple<Registrations...> registrations)
        : name_{std::move(name)},
          solution_manager_spec_{std::move(solution_manager_spec)},
          neighborhood_spec_{std::move(neighborhood_spec)},
          registrations_{std::move(registrations)}
    {
    }

    /// The name of the app.
    [[nodiscard]]
    std::string_view name() const noexcept
    {
        return name_;
    }

    /// The names of the registered runners and pipelines, in the order of
    /// registration: the names tools list and run.
    [[nodiscard]]
    std::vector<std::string_view> runner_names() const
    {
        return std::apply(
            [](const auto&... registration) {
                return std::vector<std::string_view>{
                    std::string_view{registration.name}...};
            },
            registrations_);
    }

    /// The app with spec as its SolutionManager recipe: the method spelling of
    /// `app | spec`.
    template<class Spec>
        requires std::same_as<SMSpec, detail::unconfigured_t>
        && detail::is_solution_manager_spec_v<std::remove_cvref_t<Spec>>
    [[nodiscard]]
    auto with_solution_manager(Spec&& spec) &&
    {
        using spec_type = std::remove_cvref_t<Spec>;
        static_assert(detail::validate_solution_manager_spec<spec_type>());
        return App<spec_type, NHESpec, Registrations...>{
            std::move(name_),
            std::forward<Spec>(spec),
            std::move(neighborhood_spec_),
            std::move(registrations_),
        };
    }

    /// The app with spec as its neighborhood recipe, the default of its
    /// runners: the method spelling of `app | spec`.
    template<class Spec>
        requires(!std::same_as<SMSpec, detail::unconfigured_t>)
        && std::same_as<NHESpec, detail::unconfigured_t>
        && detail::is_neighborhood_spec_v<std::remove_cvref_t<Spec>>
    [[nodiscard]]
    auto with_neighborhood(Spec&& spec) &&
    {
        using spec_type = std::remove_cvref_t<Spec>;
        return App<SMSpec, spec_type, Registrations...>{
            std::move(name_),
            std::move(solution_manager_spec_),
            std::forward<Spec>(spec),
            std::move(registrations_),
        };
    }

    /// The app with a runner of Algorithm registered as name, on the app's
    /// neighborhood: the method spelling of `app | runner<Algorithm>(name,
    /// parameters)`.
    template<detail::configurable_app_algorithm Algorithm>
        requires(!std::same_as<SMSpec, detail::unconfigured_t>)
        && (!std::same_as<NHESpec, detail::unconfigured_t>)
    [[nodiscard]]
    auto with_runner(
        std::string name,
        typename Algorithm::parameters_type parameters = {}) &&
    {
        return std::move(*this).with_runner(
            detail::app_runner_registration<Algorithm>{
                .name = std::move(name),
                .config = std::move(parameters),
                .neighborhood = {},
            });
    }

    /// The app with a runner of Algorithm registered as name, on its own
    /// neighborhood, built from the recipe neighborhood over the app's
    /// SolutionManager: the method spelling of `app | runner<Algorithm>(name,
    /// parameters, neighborhood)`.
    template<detail::configurable_app_algorithm Algorithm, class Spec>
        requires(!std::same_as<SMSpec, detail::unconfigured_t>)
        && (!std::same_as<NHESpec, detail::unconfigured_t>)
        && detail::is_neighborhood_spec_v<std::remove_cvref_t<Spec>>
    [[nodiscard]]
    auto with_runner(
        std::string name,
        typename Algorithm::parameters_type parameters,
        Spec&& neighborhood) &&
    {
        return std::move(*this).with_runner(
            detail::app_runner_registration<Algorithm, std::remove_cvref_t<Spec>>{
                .name = std::move(name),
                .config = std::move(parameters),
                .neighborhood = std::forward<Spec>(neighborhood),
            });
    }

    /// The app with a runner registration, made by easylocal::runner.
    ///
    /// A registration with its own neighborhood is checked at compile time:
    /// the neighborhood explores the app's solutions, and the algorithm runs
    /// on it.
    template<class Algorithm, class Spec>
        requires(!std::same_as<SMSpec, detail::unconfigured_t>)
        && (!std::same_as<NHESpec, detail::unconfigured_t>)
    [[nodiscard]]
    auto with_runner(detail::app_runner_registration<Algorithm, Spec> registration) &&
    {
        using registration_type = detail::app_runner_registration<Algorithm, Spec>;
        if constexpr (detail::has_own_neighborhood_v<registration_type>)
            static_assert(detail::validate_app_runner<
                Algorithm,
                detail::service_t<SMSpec>,
                Spec>());
        auto registrations = std::tuple_cat(
            std::move(registrations_),
            std::tuple<registration_type>{std::move(registration)});

        return App<SMSpec, NHESpec, Registrations..., registration_type>{
            std::move(name_),
            std::move(solution_manager_spec_),
            std::move(neighborhood_spec_),
            std::move(registrations),
        };
    }

    /// The app with a pipeline registration, made by easylocal::pipeline: the
    /// method spelling of `app | pipeline(name, ...)`.
    ///
    /// Requires a pipeline with the app's Input and Solution, and its last
    /// stage with the app's cost.
    template<class Pipeline>
        requires(!std::same_as<SMSpec, detail::unconfigured_t>)
        && (!std::same_as<NHESpec, detail::unconfigured_t>)
    [[nodiscard]]
    auto with_pipeline(detail::app_pipeline_registration<Pipeline> registration) &&
    {
        using solution_manager_type = detail::service_t<SMSpec>;
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
        using registration_type = detail::app_pipeline_registration<Pipeline>;
        auto registrations = std::tuple_cat(
            std::move(registrations_),
            std::tuple<registration_type>{std::move(registration)});

        return App<SMSpec, NHESpec, Registrations..., registration_type>{
            std::move(name_),
            std::move(solution_manager_spec_),
            std::move(neighborhood_spec_),
            std::move(registrations),
        };
    }

    /// The parameters of the runner of Algorithm registered as name, to read
    /// or change from code; const when the app is.
    ///
    /// Every run reads them, so a change applies from the next run. Throws
    /// std::invalid_argument when no runner of Algorithm has that name.
    template<class Algorithm, class Self>
        requires(detail::app_runner_count_v<Algorithm, Registrations...> > 0)
    [[nodiscard]]
    auto& runner_parameters(this Self&& self, const std::string_view name)
    {
        using parameters_type = std::conditional_t<
            std::is_const_v<std::remove_reference_t<Self>>,
            const typename Algorithm::parameters_type,
            typename Algorithm::parameters_type>;
        return detail::visit_runner_registration_named<parameters_type&, Algorithm>(
            self.registrations_,
            name,
            [](auto& registration) -> parameters_type& { return registration.config; });
    }

    /// The parameters of the app: its cost expression ("cost"), its
    /// neighborhood ("neighborhood") and each registered runner's
    /// (`runners.<name>`, for parameters that are a parameter block, and
    /// `runners.<name>.neighborhood` for its own neighborhood).
    ///
    /// Every run reads them, so a change applies from the next run; read-only
    /// when the app is const. The set refers to this app, which must stay in
    /// place while it is used: a temporary app has no configuration(). Throws
    /// std::invalid_argument when the registration names are not valid
    /// (check_registration_names).
    template<class Self>
    [[nodiscard]]
    config::parameter_set configuration(this Self& self)
    {
        self.check_registration_names();
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

    /// Throws std::invalid_argument unless the names of the runners and
    /// pipelines are valid: non-empty, distinct, and made of letters, digits,
    /// '_' and '-', since each is the key of a registration and a segment of
    /// its parameter paths (`runners.<name>.*`).
    void check_registration_names() const
    {
        const auto names = runner_names();
        for (std::size_t index = 0; index < names.size(); ++index)
        {
            const auto name = names[index];
            if (name.empty())
            {
                throw std::invalid_argument{
                    "app " + name_ + ": a runner or pipeline needs a name"};
            }
            const auto path_character = [](const char character) {
                return (character >= 'a' && character <= 'z')
                    || (character >= 'A' && character <= 'Z')
                    || (character >= '0' && character <= '9') || character == '_'
                    || character == '-';
            };
            if (!std::ranges::all_of(name, path_character))
            {
                throw std::invalid_argument{
                    "app " + name_ + ": the runner or pipeline name '" + std::string{name}
                    + "' may contain only letters, digits, '_' and '-'"};
            }
            if (std::ranges::find(names.begin(), names.begin() + index, name)
                != names.begin() + index)
            {
                throw std::invalid_argument{
                    "app " + name_ + ": two runners or pipelines are named '"
                    + std::string{name} + "'"};
            }
        }
    }

    /// A standalone Runner of the runner of Algorithm registered as name: its
    /// parameters, the app's SolutionManager recipe and the neighborhood
    /// recipe it runs on, for a solver.
    ///
    /// Throws std::invalid_argument when no runner of Algorithm has that name.
    /// Requires the runners of Algorithm to run on the same neighborhood
    /// recipe type.
    template<class Algorithm>
        requires(detail::app_runner_count_v<Algorithm, Registrations...> > 0)
    [[nodiscard]]
    auto make_runner(const std::string_view name) const
    {
        using runner_type = decltype(make_runner_of(
            std::declval<const std::tuple_element_t<
                detail::app_runner_index_impl<Algorithm, 0, Registrations...>(),
                std::tuple<Registrations...>>&>()));
        return detail::visit_runner_registration_named<runner_type, Algorithm>(
            registrations_,
            name,
            [&](const auto& registration) -> runner_type {
                static_assert(
                    std::same_as<decltype(make_runner_of(registration)), runner_type>,
                    "make_runner<Algorithm>(name) requires the runners of Algorithm "
                    "to run on the same neighborhood recipe type");
                return make_runner_of(registration);
            });
    }

    /// The app bound to input: its services built for it, which it borrows (a
    /// temporary Input is rejected).
    ///
    /// Throws std::invalid_argument when the registration names are not valid
    /// (check_registration_names).
    template<class Spec = SMSpec>
        requires complete<Spec> && Spec::template
    constructible_from<const typename detail::service_t<Spec>::input_type>&& NHESpec::
        template constructible_from<detail::service_t<Spec>> [[nodiscard]]
        auto bind(const typename detail::service_t<Spec>::input_type& input) const
    {
        check_registration_names();
        return BoundApp<SMSpec, NHESpec, Registrations...>{
            input,
            solution_manager_spec_,
            neighborhood_spec_,
            registrations_,
        };
    }

    /// Deleted: a bound app refers to its Input, which a temporary is not.
    template<class Spec = SMSpec>
        requires complete<Spec>
    auto bind(typename detail::service_t<Spec>::input_type&&) const = delete;

    /// Deleted: a bound app refers to its Input, which a temporary is not.
    template<class Spec = SMSpec>
        requires complete<Spec>
    auto bind(const typename detail::service_t<Spec>::input_type&&) const = delete;

    /// Runs the runner or the pipeline registered under name from solution, on
    /// the app freshly bound to input, and returns its result; empty when
    /// nothing has that name.
    ///
    /// It is BoundApp::run on a bound app of its own, with the current
    /// parameters: the app and the Input may be shared by concurrent runs,
    /// which do not share state. The runner gets rng when its algorithm takes
    /// one; the options are run options, such as with(control, tracer). This
    /// is how tools run the runner a user picks. The solution must be valid
    /// for the Input.
    template<std::uniform_random_bit_generator RNG, class Spec = SMSpec, class... Options>
        requires complete<Spec>
    [[nodiscard]]
    auto run(
        const std::string_view name,
        const typename detail::service_t<Spec>::input_type& input,
        typename detail::service_t<Spec>::solution_type solution,
        RNG& rng,
        Options&&... options) const
    {
        auto bound = bind(input);
        return bound
            .run(name, std::move(solution), rng, std::forward<Options>(options)...);
    }

    /// Adds a SolutionManager recipe to the app: `app | spec`.
    template<class Spec>
        requires requires(App builder, Spec&& spec) {
            std::move(builder).with_solution_manager(std::forward<Spec>(spec));
        }
    [[nodiscard]]
    friend auto operator|(App builder, Spec&& spec)
    {
        return std::move(builder).with_solution_manager(std::forward<Spec>(spec));
    }

    /// Adds the neighborhood recipe to the app: `app | spec`.
    template<class Spec>
        requires requires(App builder, Spec&& spec) {
            std::move(builder).with_neighborhood(std::forward<Spec>(spec));
        }
    [[nodiscard]]
    friend auto operator|(App builder, Spec&& spec)
    {
        return std::move(builder).with_neighborhood(std::forward<Spec>(spec));
    }

    /// Registers a runner in the app: `app | runner<Algorithm>(name, ...)`.
    template<class Algorithm, class Spec>
        requires requires(
            App builder,
            detail::app_runner_registration<Algorithm, Spec> registration) {
            std::move(builder).with_runner(std::move(registration));
        }
    [[nodiscard]]
    friend auto operator|(
        App builder,
        detail::app_runner_registration<Algorithm, Spec> registration)
    {
        return std::move(builder).with_runner(std::move(registration));
    }

    /// Registers a pipeline in the app: `app | pipeline(name, ...)`.
    template<class Pipeline>
    [[nodiscard]]
    friend auto operator|(
        App builder,
        detail::app_pipeline_registration<Pipeline> registration)
    {
        return std::move(builder).with_pipeline(std::move(registration));
    }

private:
    friend struct detail::app_access;

    template<class Registration>
    static void add_runner_configuration(
        config::parameter_set& parameters,
        Registration& registration)
    {
        using registration_type = std::remove_const_t<Registration>;
        if constexpr (detail::is_pipeline_registration_v<registration_type>)
        {
            parameters.add(
                "runners." + registration.name,
                registration.pipeline.configuration());
        }
        else
        {
            using parameters_type = typename registration_type::config_type;
            if constexpr (config::parameter_block<parameters_type>)
                parameters.add("runners." + registration.name, registration.config);
            if constexpr (detail::has_own_neighborhood_v<registration_type>)
            {
                config::add_configuration(
                    parameters,
                    "runners." + registration.name + ".neighborhood",
                    registration.neighborhood);
            }
        }
    }

    // The standalone Runner of a runner registration.
    template<class Registration>
    [[nodiscard]]
    auto make_runner_of(const Registration& registration) const
    {
        auto runner =
            easylocal::make_runner<typename Registration::algorithm_type>(
                registration.config)
            | solution_manager_spec_;
        if constexpr (detail::has_own_neighborhood_v<Registration>)
            return std::move(runner) | registration.neighborhood;
        else
            return std::move(runner) | neighborhood_spec_;
    }

    std::string name_;
    EASYLOCAL_NO_UNIQUE_ADDRESS SMSpec solution_manager_spec_{};
    EASYLOCAL_NO_UNIQUE_ADDRESS NHESpec neighborhood_spec_{};
    std::tuple<Registrations...> registrations_{};
};

namespace detail
{

// The members of App and BoundApp keyed by algorithm type: for a type
// registered once, and for the library's own tools and tests.
struct app_access
{
    // The parameters of the only runner of Algorithm, const when the app is.
    template<class Algorithm, class AppType>
    [[nodiscard]]
    static auto& runner_parameters(AppType& application) noexcept
    {
        using registrations_type =
            std::remove_const_t<decltype(application.registrations_)>;
        return std::get<app_runner_index_in<Algorithm, registrations_type>::value>(
            application.registrations_)
            .config;
    }

    // Visits the runners (not the pipelines), in the order they were
    // registered: visitor.template operator()<Algorithm>(name, parameters).
    template<class SM, class NH, class... Rs, class Visitor>
    static void for_each_runner(const App<SM, NH, Rs...>& application, Visitor&& visitor)
    {
        std::apply(
            [&](const auto&... registration) {
                (visit_runner(registration, visitor), ...);
            },
            application.registrations_);
    }

    // Visits the runners that have their own neighborhood, on a bound app:
    // visitor(name, neighborhood).
    template<class SM, class NH, class... Rs, class Visitor>
    static void for_each_own_neighborhood(
        const BoundApp<SM, NH, Rs...>& bound,
        Visitor&& visitor)
    {
        [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            (
                [&] {
                    using registration_type =
                        std::tuple_element_t<Index, std::tuple<Rs...>>;
                    if constexpr (has_own_neighborhood_v<registration_type>)
                    {
                        visitor(
                            std::string_view{std::get<Index>(bound.registrations_).name},
                            std::get<Index>(bound.own_neighborhoods_).value);
                    }
                }(),
                ...);
        }(std::index_sequence_for<Rs...>{});
    }

    // The only runner of Algorithm on a bound app, on its neighborhood.
    template<class Algorithm, class SM, class NH, class... Rs>
    [[nodiscard]]
    static auto runner(BoundApp<SM, NH, Rs...>& bound)
    {
        return bound.template runner_at<app_runner_index<Algorithm, Rs...>()>();
    }

    // Runs the only runner of Algorithm of a bound app, with its own result
    // type: run(solution, args...).
    template<class Algorithm, class SM, class NH, class... Rs, class... RunArgs>
    [[nodiscard]]
    static auto run(
        BoundApp<SM, NH, Rs...>& bound,
        typename BoundApp<SM, NH, Rs...>::solution_type solution,
        RunArgs&&... args)
    {
        return runner<Algorithm>(bound).run(
            std::move(solution),
            std::forward<RunArgs>(args)...);
    }

    // Runs the only runner of Algorithm on an app freshly bound to input.
    template<class Algorithm, class SM, class NH, class... Rs, class... RunArgs>
    [[nodiscard]]
    static auto run(
        const App<SM, NH, Rs...>& application,
        const typename service_t<SM>::input_type& input,
        typename service_t<SM>::solution_type solution,
        RunArgs&&... args)
    {
        auto bound = application.bind(input);
        return run<Algorithm>(bound, std::move(solution), std::forward<RunArgs>(args)...);
    }

private:
    template<class Registration, class Visitor>
    static void visit_runner(const Registration& registration, Visitor& visitor)
    {
        if constexpr (!is_pipeline_registration_v<Registration>)
        {
            visitor.template operator()<typename Registration::algorithm_type>(
                std::string_view{registration.name},
                registration.config);
        }
    }
};

} // namespace detail

/// Starts an app named name, without components: a SolutionManager recipe, a
/// neighborhood recipe and runners are added to it with `|`.
[[nodiscard]]
inline App<detail::unconfigured_t, detail::unconfigured_t> app(std::string name)
{
    return App<detail::unconfigured_t, detail::unconfigured_t>{std::move(name)};
}

/// A runner registration for an app, on the app's neighborhood: `app("tsp") |
/// sm | nhe | runner<runners::FirstImprovement>("fi", {...})`.
///
/// The name is its key, and the segment of its parameter paths
/// (`runners.<name>.*`). Requires an algorithm with a default-constructible
/// parameters_type, constructible from it.
template<detail::configurable_app_algorithm Algorithm>
[[nodiscard]]
detail::app_runner_registration<Algorithm> runner(
    std::string name,
    typename Algorithm::parameters_type parameters = {})
{
    return {.name = std::move(name), .config = std::move(parameters), .neighborhood = {}};
}

/// A runner registration for an app, on its own neighborhood: `runner<SA>("sa",
/// {...}, neighborhood<Swap>() | delta<...>())`.
///
/// The neighborhood is built from its recipe over the app's SolutionManager,
/// next to the app's own; its parameters are under
/// `runners.<name>.neighborhood.*`. Requires an algorithm with a
/// default-constructible parameters_type, constructible from it, and a
/// neighborhood recipe.
template<detail::configurable_app_algorithm Algorithm, class Spec>
    requires detail::is_neighborhood_spec_v<std::remove_cvref_t<Spec>>
[[nodiscard]]
detail::app_runner_registration<Algorithm, std::remove_cvref_t<Spec>> runner(
    std::string name,
    typename Algorithm::parameters_type parameters,
    Spec&& neighborhood)
{
    return {
        .name = std::move(name),
        .config = std::move(parameters),
        .neighborhood = std::forward<Spec>(neighborhood),
    };
}

/// A pipeline registration for an app, of a pipeline already built:
/// `pipeline("cascade", (stage(...) & until_feasible()) | stage(...))`.
template<std::uniform_random_bit_generator RNG, class... Stages>
[[nodiscard]]
detail::app_pipeline_registration<solvers::Pipeline<RNG, Stages...>> pipeline(
    std::string name,
    solvers::Pipeline<RNG, Stages...> pipeline)
{
    return {.name = std::move(name), .pipeline = std::move(pipeline)};
}

/// A pipeline registration for an app, run by name from the current solution
/// like a runner: `app("tsp") | sm | nhe | pipeline("cascade", stage(...),
/// stage(...))`.
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
