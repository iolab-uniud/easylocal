#pragma once

/// \file
/// Runner: a search algorithm composed with a SolutionManager recipe and a
/// neighborhood recipe (make_runner).
///
/// Binding it to an Input builds the services (BoundRunner) on which the
/// algorithm runs; its parameters (search, cost, neighborhood) are exposed as
/// one parameter_set.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost.hpp>
#include <easylocal/cost/semantics.hpp>
#include <easylocal/helpers/detail/evaluation.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/recipes.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/trace/events.hpp>
#include <easylocal/trace/tracer.hpp>

#include <cassert>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal
{

namespace detail
{

struct unconfigured_t
{
};

template<class NHE, class SM>
concept runner_neighborhood_explorer =
    detail::evaluable_solution_manager<SM> &&
    easylocal::neighborhood_explorer_for<NHE, SM>;

template<class NHE, class SM>
concept enumerable_runner_neighborhood_explorer =
    runner_neighborhood_explorer<NHE, SM> &&
    requires(
        const NHE& neighborhood,
        const typename SM::solution_type& solution)
    {
        {
            easylocal::moves(neighborhood, solution)
        } -> easylocal::move_input_range_for<typename NHE::move_type>;
    };

template<class SM, class NHE>
    requires runner_neighborhood_explorer<NHE, SM>
class runner_context
{
public:
    using input_type = typename SM::input_type;
    using solution_type = typename SM::solution_type;
    using cost_type = typename SM::cost_type;
    using solution_manager_type = SM;
    using neighborhood_explorer_type = NHE;

    runner_context(const SM& solution_manager, const NHE& neighborhood) noexcept
        : solution_manager_{solution_manager}, neighborhood_{neighborhood}
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
    const input_type& input() const noexcept
    {
        return solution_manager_.input();
    }

    [[nodiscard]]
    evaluation_facility<SM, NHE> evaluation() const
    {
        return evaluation_facility<SM, NHE>{
            solution_manager_,
            neighborhood_,
        };
    }

    // Semantic cost queries used by search algorithms. A cost::apply at the
    // root of the cost expression may override the meaning of these
    // relations; otherwise the ordinary cost operators provide them.
    //
    // These are deliberately distinct queries: better_or_equivalent() is not
    // better() || equivalent(), so a cost model may answer <= in one pass
    // rather than with two potentially expensive comparisons.
    [[nodiscard]]
    constexpr bool better(const cost_type& candidate, const cost_type& reference) const
        requires easylocal::cost::has_better<SM>
    {
        return easylocal::cost::better(
            solution_manager_,
            candidate,
            reference);
    }

    [[nodiscard]]
    constexpr bool equivalent(const cost_type& lhs, const cost_type& rhs) const
        requires easylocal::cost::has_equivalent<SM>
    {
        return easylocal::cost::equivalent(solution_manager_, lhs, rhs);
    }

    [[nodiscard]]
    constexpr bool better_or_equivalent(
        const cost_type& candidate,
        const cost_type& reference) const
        requires easylocal::cost::has_better_or_equivalent<SM>
    {
        return easylocal::cost::better_or_equivalent(
            solution_manager_,
            candidate,
            reference);
    }

private:
    const SM& solution_manager_;
    const NHE& neighborhood_;
};

template<class T>
inline constexpr bool is_run_options_v = false;

template<class Tracer, class Target>
inline constexpr bool is_run_options_v<run_options<Tracer, Target>> = true;

// Splits Runner::run() arguments into the algorithm arguments and an optional
// trailing run_options.
template<class... Args>
struct run_arguments
{
    static constexpr bool has_options = false;
    static constexpr std::size_t forwarded_count = sizeof...(Args);
    using tracer_type = trace::null_tracer;
};

template<class First, class... Rest>
    requires is_run_options_v<std::remove_cvref_t<
        std::tuple_element_t<sizeof...(Rest), std::tuple<First, Rest...>>>>
struct run_arguments<First, Rest...>
{
    static constexpr bool has_options = true;
    static constexpr std::size_t forwarded_count = sizeof...(Rest);
    using tracer_type = typename std::remove_cvref_t<
        std::tuple_element_t<sizeof...(Rest), std::tuple<First, Rest...>>>::tracer_type;
};

template<class Algorithm, class Run, class Solution, class ArgsTuple, class Sequence>
inline constexpr bool algorithm_runnable_impl = false;

template<class Algorithm, class Run, class Solution, class ArgsTuple, std::size_t... Index>
inline constexpr bool algorithm_runnable_impl<
    Algorithm,
    Run,
    Solution,
    ArgsTuple,
    std::index_sequence<Index...>> =
    requires(Algorithm& algorithm, Run& run, Solution solution) {
        algorithm.run(
            run,
            std::move(solution),
            std::declval<std::tuple_element_t<Index, ArgsTuple>>()...);
    };

template<class Algorithm, class Context, class... Args>
concept algorithm_runnable = algorithm_runnable_impl<
    Algorithm,
    search_run<Context, typename run_arguments<Args...>::tracer_type>,
    typename Context::solution_type,
    std::tuple<Args&&...>,
    std::make_index_sequence<run_arguments<Args...>::forwarded_count>>;

template<class Algorithm, class Context, class... Args>
    requires algorithm_runnable<Algorithm, Context, Args...>
[[nodiscard]]
auto run_algorithm(
    Algorithm& algorithm,
    const Context& context,
    typename Context::solution_type solution,
    Args&&... args)
{
    using arguments = run_arguments<Args...>;
    using tracer_type = typename arguments::tracer_type;

    auto forwarded = std::forward_as_tuple(std::forward<Args>(args)...);
    const run_control default_control{};
    trace::null_tracer null_tracer;

    const run_control* control = &default_control;
    tracer_type* tracer = nullptr;
    if constexpr (arguments::has_options)
    {
        const auto& options = std::get<sizeof...(Args) - 1>(forwarded);
        if (options.control != nullptr)
        {
            control = options.control;
        }
        tracer = options.tracer;
    }
    if constexpr (std::same_as<tracer_type, trace::null_tracer>)
    {
        tracer = &null_tracer;
    }
    assert(tracer != nullptr);

    using cost_type = typename Context::cost_type;
    std::optional<cost_type> target;
    if constexpr (arguments::has_options)
    {
        const auto& options = std::get<sizeof...(Args) - 1>(forwarded);
        using options_type = std::remove_cvref_t<decltype(options)>;
        if constexpr (!std::same_as<typename options_type::target_type, no_target>)
        {
            static_assert(
                std::constructible_from<cost_type, const typename options_type::target_type&>,
                "the target cost of the run options must convert to the runner's cost type");
            if (options.target)
            {
                target.emplace(*options.target);
            }
        }
    }

    // The time limit starts now, with the run.
    std::optional<std::chrono::steady_clock::time_point> deadline;
    if constexpr (arguments::has_options)
    {
        const auto& options = std::get<sizeof...(Args) - 1>(forwarded);
        if (options.time_limit)
            deadline = detail::deadline_after(*options.time_limit);
    }

    // The caller's evaluation budget, which the runner's own may tighten, and
    // what the archive keeps.
    std::size_t evaluation_limit = search_run<Context, tracer_type>::no_evaluation_limit;
    pareto_archive_parameters front;
    if constexpr (arguments::has_options)
    {
        const auto& options = std::get<sizeof...(Args) - 1>(forwarded);
        if (options.evaluation_budget)
            evaluation_limit = *options.evaluation_budget;
        front = options.front;
    }

    // The run refers to the target, which outlives it.
    search_run<Context, tracer_type> run{
        context,
        *control,
        *tracer,
        evaluation_limit,
        target ? &*target : nullptr,
        deadline,
        front};
    return [&]<std::size_t... Index>(std::index_sequence<Index...>) {
        return algorithm.run(
            run,
            std::move(solution),
            std::get<Index>(std::move(forwarded))...);
    }(std::make_index_sequence<arguments::forwarded_count>{});
}

} // namespace detail

/// A runner bound to an Input: the services built for it (a SolutionManager
/// and a neighborhood explorer) and the algorithm, which run(solution, ...)
/// runs on them.
///
/// Runner::bind returns it; it borrows the Input, which must outlive it, and
/// cannot be copied or moved, since its services refer to each other. Requires
/// a SolutionManager recipe with a cost and a neighborhood recipe for it.
template<class Algorithm, class SMSpec, class NHESpec>
    requires detail::is_solution_manager_spec_v<SMSpec>
    && detail::is_neighborhood_spec_v<NHESpec>
    && detail::runner_neighborhood_explorer<
        detail::service_t<NHESpec>,
        detail::service_t<SMSpec>>
class BoundRunner
{
public:
    /// The SolutionManager built from the recipe.
    using solution_manager_type = detail::service_t<SMSpec>;
    /// The neighborhood explorer built from the recipe.
    using neighborhood_explorer_type = detail::service_t<NHESpec>;
    /// The Input of the problem.
    using input_type = typename solution_manager_type::input_type;
    /// The Solution of the problem.
    using solution_type = typename solution_manager_type::solution_type;
    /// The cost of a solution.
    using cost_type = typename solution_manager_type::cost_type;

    static_assert(detail::validate_delta_bindings<
        solution_manager_type,
        neighborhood_explorer_type>());

    /// The services of the recipes built for input, with the algorithm.
    BoundRunner(
        Algorithm algorithm,
        const input_type& input,
        const SMSpec& solution_manager_spec,
        const NHESpec& neighborhood_spec)
        : algorithm_{std::move(algorithm)},
          input_{input},
          solution_manager_{solution_manager_spec.construct(input_)},
          neighborhood_{neighborhood_spec.construct(solution_manager_)}
    {
        assert(
            std::addressof(solution_manager_.input()) ==
                std::addressof(input_) &&
            "SolutionManager must bind to the requested Instance");
        assert(
            std::addressof(neighborhood_.input()) ==
                std::addressof(input_) &&
            "NeighborhoodExplorer must share the bound Instance");
    }

    BoundRunner(const BoundRunner&) = delete;
    BoundRunner& operator=(const BoundRunner&) = delete;
    BoundRunner(BoundRunner&&) = delete;
    BoundRunner& operator=(BoundRunner&&) = delete;

    /// The Input the runner is bound to.
    [[nodiscard]]
    const input_type& input() const noexcept
    {
        return input_;
    }

    /// The SolutionManager, for the solution identity of the solvers.
    [[nodiscard]]
    const solution_manager_type& solution_manager() const noexcept
    {
        return solution_manager_;
    }

    /// The SolutionManager's initial_solution().
    [[nodiscard]]
    solution_type initial_solution() const
        requires has_initial_solution<solution_manager_type>
    {
        return solution_manager_.initial_solution();
    }

    /// A random_solution(rng) of the SolutionManager.
    template<class RNG>
    [[nodiscard]]
    solution_type random_solution(RNG& rng) const
        requires has_random_solution<solution_manager_type, RNG>
    {
        return solution_manager_.random_solution(rng);
    }

    /// Whether candidate is better than reference, as the search compares
    /// costs.
    [[nodiscard]]
    constexpr bool better(const cost_type& candidate, const cost_type& reference) const
        requires easylocal::cost::has_better<solution_manager_type>
    {
        return easylocal::cost::better(
            solution_manager_,
            candidate,
            reference);
    }

    /// Runs the algorithm from solution and returns its result.
    ///
    /// The algorithm's arguments (such as an RNG) may be followed by run
    /// options, such as easylocal::with(control, tracer). The solution must be
    /// valid for the Input.
    template<class... RunArgs>
    [[nodiscard]]
    auto run(solution_type solution, RunArgs&&... run_args)
        requires detail::algorithm_runnable<
            Algorithm,
            detail::runner_context<solution_manager_type, neighborhood_explorer_type>,
            RunArgs...>
    {
        assert(
            solution_manager_.is_valid(solution) &&
            "initial Solution must be compatible with the bound Instance");

        const detail::runner_context<solution_manager_type, neighborhood_explorer_type>
            context{
                solution_manager_,
                neighborhood_,
        };

        return detail::run_algorithm(
            algorithm_,
            context,
            std::move(solution),
            std::forward<RunArgs>(run_args)...);
    }

private:
    Algorithm algorithm_;
    const input_type& input_;
    solution_manager_type solution_manager_;
    neighborhood_explorer_type neighborhood_;
};

namespace detail
{

// An algorithm whose parameters are a parameter block it is built from: a
// Runner holds the parameters and builds the algorithm when it is bound.
template<class Algorithm>
concept parameterized_algorithm = requires { typename Algorithm::parameters_type; }
    && config::parameter_block<typename Algorithm::parameters_type>
    && std::constructible_from<Algorithm, const typename Algorithm::parameters_type&>;

// What a Runner holds of its algorithm: the algorithm itself, or, for a
// parameterized algorithm, its parameters, from which it is built at bind.
template<class Algorithm>
class algorithm_source
{
public:
    explicit algorithm_source(Algorithm algorithm) : algorithm_{std::move(algorithm)} {}

    [[nodiscard]]
    Algorithm make() const&
        requires std::copy_constructible<Algorithm>
    {
        return algorithm_;
    }

    [[nodiscard]]
    Algorithm make() &&
    {
        return std::move(algorithm_);
    }

    template<class Self>
    void add_configuration(this Self&& self, config::parameter_set& parameters)
    {
        config::add_configuration(parameters, "search", self.algorithm_);
    }

private:
    Algorithm algorithm_;
};

template<parameterized_algorithm Algorithm>
class algorithm_source<Algorithm>
{
public:
    using parameters_type = typename Algorithm::parameters_type;

    explicit algorithm_source(parameters_type parameters)
        : parameters_{std::move(parameters)}
    {
        assert(parameters_.validate() && "the runner's parameters must be valid");
    }

    [[nodiscard]]
    Algorithm make() const
    {
        return Algorithm{parameters_};
    }

    template<class Self>
    [[nodiscard]]
    auto& parameters(this Self&& self) noexcept
    {
        return self.parameters_;
    }

    template<class Self>
    void add_configuration(this Self&& self, config::parameter_set& parameters)
    {
        parameters.add("search", self.parameters_);
    }

private:
    parameters_type parameters_;
};

} // namespace detail

/// A search algorithm with the recipes of the services it runs on: a
/// SolutionManager (with its cost expression) and a neighborhood explorer.
///
/// It is composed step by step, make_runner<Algorithm>(parameters) | sm_recipe
/// | nhe_recipe, each step a specialization; bind(input) then builds the
/// services for an Input and returns the bound runner, whose run(solution, ...)
/// runs the algorithm.
template<
    class Algorithm,
    class SMSpec = detail::unconfigured_t,
    class NHESpec = detail::unconfigured_t>
class Runner;

/// A runner with its algorithm only; with_solution_manager(), or |, adds the
/// SolutionManager recipe.
template<class Algorithm>
class Runner<Algorithm, detail::unconfigured_t, detail::unconfigured_t>
{
public:
    /// An algorithm without a parameter block, held as it is.
    explicit Runner(Algorithm algorithm)
        requires(!detail::parameterized_algorithm<Algorithm>)
        : algorithm_{std::move(algorithm)}
    {
    }

    /// A parameterized algorithm is built from its parameters when the runner
    /// is bound: make_runner<Algorithm>(parameters) creates the runner.
    template<class Self = Algorithm>
        requires detail::parameterized_algorithm<Self>
    explicit Runner(Algorithm)
    {
        static_assert(
            !detail::parameterized_algorithm<Self>,
            "a Runner holds the parameters of this algorithm: create it with "
            "make_runner<Algorithm>(parameters)");
    }

    explicit Runner(detail::algorithm_source<Algorithm> algorithm)
        : algorithm_{std::move(algorithm)}
    {
    }

    template<class SMSpec>
        requires detail::is_solution_manager_spec_v<std::remove_cvref_t<SMSpec>> &&
                 std::copy_constructible<Algorithm> &&
                 std::constructible_from<
                     std::remove_cvref_t<SMSpec>,
                     SMSpec&&>
    [[nodiscard]]
    auto with_solution_manager(SMSpec&& spec) const &
    {
        using spec_type = std::remove_cvref_t<SMSpec>;
        static_assert(detail::validate_solution_manager_spec<spec_type>());
        return Runner<Algorithm, spec_type>{
            algorithm_,
            std::forward<SMSpec>(spec),
        };
    }

    template<class SMSpec>
        requires detail::is_solution_manager_spec_v<std::remove_cvref_t<SMSpec>> &&
                 std::constructible_from<
                     std::remove_cvref_t<SMSpec>,
                     SMSpec&&>
    [[nodiscard]]
    auto with_solution_manager(SMSpec&& spec) &&
    {
        using spec_type = std::remove_cvref_t<SMSpec>;
        static_assert(detail::validate_solution_manager_spec<spec_type>());
        return Runner<Algorithm, spec_type>{
            std::move(algorithm_),
            std::forward<SMSpec>(spec),
        };
    }

private:
    detail::algorithm_source<Algorithm> algorithm_;
};

/// A runner with its algorithm and SolutionManager recipe; with_neighborhood(),
/// or |, adds the neighborhood recipe.
template<class Algorithm, class SMSpec>
    requires detail::is_solution_manager_spec_v<SMSpec>
class Runner<Algorithm, SMSpec, detail::unconfigured_t>
{
public:
    using solution_manager_type = detail::service_t<SMSpec>;

    Runner(detail::algorithm_source<Algorithm> algorithm, SMSpec solution_manager_spec)
        : algorithm_{std::move(algorithm)},
          solution_manager_spec_{std::move(solution_manager_spec)}
    {
    }

    template<class NHESpec>
        requires detail::is_neighborhood_spec_v<std::remove_cvref_t<NHESpec>> &&
                 detail::runner_neighborhood_explorer<
                     detail::service_t<std::remove_cvref_t<NHESpec>>,
                     solution_manager_type> &&
                 std::copy_constructible<Algorithm> &&
                 std::copy_constructible<SMSpec> &&
                 std::constructible_from<
                     std::remove_cvref_t<NHESpec>,
                     NHESpec&&>
    [[nodiscard]]
    auto with_neighborhood(NHESpec&& spec) const &
    {
        using spec_type = std::remove_cvref_t<NHESpec>;

        return Runner<Algorithm, SMSpec, spec_type>{
            algorithm_,
            solution_manager_spec_,
            std::forward<NHESpec>(spec),
        };
    }

    template<class NHESpec>
        requires detail::is_neighborhood_spec_v<std::remove_cvref_t<NHESpec>> &&
                 detail::runner_neighborhood_explorer<
                     detail::service_t<std::remove_cvref_t<NHESpec>>,
                     solution_manager_type> &&
                 std::constructible_from<
                     std::remove_cvref_t<NHESpec>,
                     NHESpec&&>
    [[nodiscard]]
    auto with_neighborhood(NHESpec&& spec) &&
    {
        using spec_type = std::remove_cvref_t<NHESpec>;

        return Runner<Algorithm, SMSpec, spec_type>{
            std::move(algorithm_),
            std::move(solution_manager_spec_),
            std::forward<NHESpec>(spec),
        };
    }

private:
    detail::algorithm_source<Algorithm> algorithm_;
    SMSpec solution_manager_spec_;
};

/// A complete runner: bind(input) builds its services for an Input.
template<class Algorithm, class SMSpec, class NHESpec>
    requires detail::is_solution_manager_spec_v<SMSpec> &&
             detail::is_neighborhood_spec_v<NHESpec> &&
             detail::runner_neighborhood_explorer<
                 detail::service_t<NHESpec>,
                 detail::service_t<SMSpec>>
class Runner<Algorithm, SMSpec, NHESpec>
{
public:
    using solution_manager_type = detail::service_t<SMSpec>;
    using neighborhood_explorer_type = detail::service_t<NHESpec>;
    using input_type = typename solution_manager_type::input_type;

    Runner(
        detail::algorithm_source<Algorithm> algorithm,
        SMSpec solution_manager_spec,
        NHESpec neighborhood_spec)
        : algorithm_{std::move(algorithm)},
          solution_manager_spec_{std::move(solution_manager_spec)},
          neighborhood_spec_{std::move(neighborhood_spec)}
    {
    }

    /// The algorithm's parameters, to read or change from code; the algorithm
    /// is built from them when the runner is bound.
    [[nodiscard]]
    auto& parameters() noexcept
        requires detail::parameterized_algorithm<Algorithm>
    {
        return algorithm_.parameters();
    }

    [[nodiscard]]
    const auto& parameters() const noexcept
        requires detail::parameterized_algorithm<Algorithm>
    {
        return algorithm_.parameters();
    }

    /// The parameters of the algorithm ("search"), of the cost expression
    /// ("cost") and of the neighborhood ("neighborhood"), with paths relative
    /// to the runner: whoever composes it adds a prefix, if any.
    ///
    /// The set refers to this runner, which must stay in place while it is
    /// used: a temporary runner has no configuration().
    template<class Self>
    [[nodiscard]]
    config::parameter_set configuration(this Self& self)
    {
        config::parameter_set parameters;
        self.algorithm_.add_configuration(parameters);
        config::add_configuration(parameters, "cost", self.solution_manager_spec_);
        config::add_configuration(parameters, "neighborhood", self.neighborhood_spec_);
        return parameters;
    }

    [[nodiscard]]
    auto with_hard_cost() const &
        requires detail::hierarchical_solution_manager<solution_manager_type> &&
                 std::copy_constructible<Algorithm> &&
                 std::copy_constructible<SMSpec> &&
                 std::copy_constructible<NHESpec>
    {
        using hard_sm_spec_type =
            detail::hard_cost_layer_spec<SMSpec>;

        return Runner<Algorithm, hard_sm_spec_type, NHESpec>{
            algorithm_,
            hard_sm_spec_type{solution_manager_spec_},
            neighborhood_spec_,
        };
    }

    [[nodiscard]]
    auto with_hard_cost() &&
        requires detail::hierarchical_solution_manager<solution_manager_type>
    {
        using hard_sm_spec_type =
            detail::hard_cost_layer_spec<SMSpec>;

        return Runner<Algorithm, hard_sm_spec_type, NHESpec>{
            std::move(algorithm_),
            hard_sm_spec_type{std::move(solution_manager_spec_)},
            std::move(neighborhood_spec_),
        };
    }

    [[nodiscard]]
    auto bind(const input_type& input) const &
        requires std::copy_constructible<Algorithm> &&
                 (SMSpec::template constructible_from<const input_type>) &&
                 (NHESpec::template constructible_from<solution_manager_type>)
    {
        return BoundRunner<Algorithm, SMSpec, NHESpec>{
            algorithm_.make(),
            input,
            solution_manager_spec_,
            neighborhood_spec_,
        };
    }

    [[nodiscard]]
    auto bind(const input_type& input) &&
        requires (SMSpec::template constructible_from<const input_type>) &&
                 (NHESpec::template constructible_from<solution_manager_type>)
    {
        return BoundRunner<Algorithm, SMSpec, NHESpec>{
            std::move(algorithm_).make(),
            input,
            solution_manager_spec_,
            neighborhood_spec_,
        };
    }

    auto bind(input_type&&) const & = delete;
    auto bind(const input_type&&) const & = delete;
    auto bind(input_type&&) && = delete;
    auto bind(const input_type&&) && = delete;

private:
    detail::algorithm_source<Algorithm> algorithm_;
    SMSpec solution_manager_spec_;
    NHESpec neighborhood_spec_;
};

/// runner | sm_recipe: the runner with its SolutionManager recipe.
template<class Algorithm, class SMSpec, class NHESpec, class Spec>
    requires std::same_as<SMSpec, detail::unconfigured_t> &&
             std::same_as<NHESpec, detail::unconfigured_t> &&
             detail::is_solution_manager_spec_v<std::remove_cvref_t<Spec>>
[[nodiscard]]
auto operator|(
    Runner<Algorithm, SMSpec, NHESpec> runner,
    Spec&& spec)
{
    return std::move(runner).with_solution_manager(std::forward<Spec>(spec));
}

/// runner | nhe_recipe: the runner with its neighborhood recipe.
template<class Algorithm, class SMSpec, class NHESpec>
    requires detail::is_solution_manager_spec_v<SMSpec> &&
             detail::is_neighborhood_spec_v<std::remove_cvref_t<NHESpec>>
[[nodiscard]]
auto operator|(
    Runner<Algorithm, SMSpec, detail::unconfigured_t> runner,
    NHESpec&& spec)
{
    return std::move(runner).with_neighborhood(
        std::forward<NHESpec>(spec));
}

/// Runner{algorithm} holds that algorithm.
template<class Algorithm>
Runner(Algorithm) -> Runner<std::remove_cvref_t<Algorithm>>;

/// A runner for a parameterized algorithm, from its parameters:
/// make_runner<FirstImprovement>({.max_evaluations = 1000}).
template<class Algorithm>
    requires detail::parameterized_algorithm<Algorithm>
[[nodiscard]]
auto make_runner(typename Algorithm::parameters_type parameters = {})
{
    return Runner<Algorithm>{detail::algorithm_source<Algorithm>{std::move(parameters)}};
}

/// A runner for any other algorithm, built from its arguments.
template<class Algorithm, class... Args>
    requires(!detail::parameterized_algorithm<Algorithm>)
    && std::constructible_from<Algorithm, Args&&...>
[[nodiscard]]
auto make_runner(Args&&... args)
{
    return Runner<Algorithm>{
        detail::algorithm_source<Algorithm>{Algorithm{std::forward<Args>(args)...}}};
}

} // namespace easylocal
