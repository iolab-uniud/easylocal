#pragma once

#include <easylocal/cost.hpp>
#include <easylocal/config/tree.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/cost/semantics.hpp>
#include <easylocal/helpers/detail/evaluation.hpp>
#include <easylocal/helpers/recipes.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/runners/run_control.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/trace/events.hpp>
#include <easylocal/trace/tracer.hpp>

#include <cassert>
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
    auto solution_manager() const noexcept -> const SM&
    {
        return solution_manager_;
    }

    [[nodiscard]]
    auto neighborhood_explorer() const noexcept -> const NHE&
    {
        return neighborhood_;
    }

    [[nodiscard]]
    auto input() const noexcept -> const input_type&
    {
        return solution_manager_.input();
    }

    [[nodiscard]]
    auto evaluation() const -> evaluation_facility<SM, NHE>
    {
        return evaluation_facility<SM, NHE>{
            solution_manager_,
            neighborhood_,
        };
    }

    // Semantic cost queries used by search algorithms. A cost::apply at the
    // root of the cost expression may override the meaning of these relations; otherwise the ordinary cost
    // operators provide the exact/default semantics.
    //
    // These are deliberately distinct queries. In particular,
    // better_or_equivalent() is not defined as better() || equivalent(), so a
    // future lazy cost model can answer <= in one pass without forcing two
    // potentially expensive semantic comparisons.
    [[nodiscard]]
    constexpr auto better(
        const cost_type& candidate,
        const cost_type& reference) const -> bool
        requires easylocal::cost::has_better<SM>
    {
        return easylocal::cost::better(
            solution_manager_,
            candidate,
            reference);
    }

    [[nodiscard]]
    constexpr auto equivalent(
        const cost_type& lhs,
        const cost_type& rhs) const -> bool
        requires easylocal::cost::has_equivalent<SM>
    {
        return easylocal::cost::equivalent(solution_manager_, lhs, rhs);
    }

    [[nodiscard]]
    constexpr auto better_or_equivalent(
        const cost_type& candidate,
        const cost_type& reference) const -> bool
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

    // The run refers to the target, which outlives it.
    search_run<Context, tracer_type> run{
        context,
        *control,
        *tracer,
        search_run<Context, tracer_type>::no_evaluation_limit,
        target ? &*target : nullptr};
    return [&]<std::size_t... Index>(std::index_sequence<Index...>) {
        return algorithm.run(
            run,
            std::move(solution),
            std::get<Index>(std::move(forwarded))...);
    }(std::make_index_sequence<arguments::forwarded_count>{});
}

template<class Algorithm, class SMSpec, class NHESpec>
    requires is_solution_manager_spec_v<SMSpec> &&
             is_neighborhood_spec_v<NHESpec> &&
             runner_neighborhood_explorer<
                 service_t<NHESpec>,
                 service_t<SMSpec>>
class bound_runner
{
public:
    using solution_manager_type = service_t<SMSpec>;
    using neighborhood_explorer_type = service_t<NHESpec>;
    using input_type = typename solution_manager_type::input_type;
    using solution_type = typename solution_manager_type::solution_type;
    using cost_type = typename solution_manager_type::cost_type;

    static_assert(validate_delta_bindings<
                  solution_manager_type,
                  neighborhood_explorer_type>());

    bound_runner(
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

    bound_runner(const bound_runner&) = delete;
    auto operator=(const bound_runner&) -> bound_runner& = delete;
    bound_runner(bound_runner&&) = delete;
    auto operator=(bound_runner&&) -> bound_runner& = delete;

    [[nodiscard]]
    auto input() const noexcept -> const input_type&
    {
        return input_;
    }

    [[nodiscard]]
    auto initial_solution() const -> solution_type
        requires has_initial_solution<solution_manager_type>
    {
        return solution_manager_.initial_solution();
    }

    template<class RNG>
    [[nodiscard]]
    auto random_solution(RNG& rng) const -> solution_type
        requires has_random_solution<solution_manager_type, RNG>
    {
        return solution_manager_.random_solution(rng);
    }

    [[nodiscard]]
    constexpr auto better(
        const cost_type& candidate,
        const cost_type& reference) const -> bool
        requires easylocal::cost::has_better<solution_manager_type>
    {
        return easylocal::cost::better(
            solution_manager_,
            candidate,
            reference);
    }

    // Runs the algorithm from the given solution. Algorithm arguments (e.g.
    // an RNG) may be followed by easylocal::with(control, tracer).
    template<class... RunArgs>
    [[nodiscard]]
    auto run(solution_type solution, RunArgs&&... run_args)
        requires algorithm_runnable<
            Algorithm,
            runner_context<solution_manager_type, neighborhood_explorer_type>,
            RunArgs...>
    {
        assert(
            solution_manager_.is_valid(solution) &&
            "initial Solution must be compatible with the bound Instance");

        const runner_context<
            solution_manager_type,
            neighborhood_explorer_type>
            context{
                solution_manager_,
                neighborhood_,
            };

        return run_algorithm(
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

} // namespace detail

template<
    class Algorithm,
    class SMSpec = detail::unconfigured_t,
    class NHESpec = detail::unconfigured_t>
class Runner;

template<class Algorithm>
class Runner<Algorithm, detail::unconfigured_t, detail::unconfigured_t>
{
public:
    explicit Runner(Algorithm algorithm)
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
    Algorithm algorithm_;
};

template<class Algorithm, class SMSpec>
    requires detail::is_solution_manager_spec_v<SMSpec>
class Runner<Algorithm, SMSpec, detail::unconfigured_t>
{
public:
    using solution_manager_type = detail::service_t<SMSpec>;

    Runner(Algorithm algorithm, SMSpec solution_manager_spec)
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
    Algorithm algorithm_;
    SMSpec solution_manager_spec_;
};

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
        Algorithm algorithm,
        SMSpec solution_manager_spec,
        NHESpec neighborhood_spec)
        : algorithm_{std::move(algorithm)},
          solution_manager_spec_{std::move(solution_manager_spec)},
          neighborhood_spec_{std::move(neighborhood_spec)}
    {
    }

    template<config::fixed_string Name>
        requires (
            config::configuration_provider<Algorithm> ||
            config::configuration_provider<SMSpec> ||
            config::configuration_provider<NHESpec>)
    [[nodiscard]]
    auto configuration()
    {
        auto children = std::tuple_cat(
            config::configuration_nodes(algorithm_),
            config::configuration_nodes(solution_manager_spec_),
            config::configuration_nodes(neighborhood_spec_));

        return std::apply(
            [](auto... nodes) {
                return config::named<Name>(std::move(nodes)...);
            },
            std::move(children));
    }

    template<config::fixed_string Name>
        requires (
            config::configuration_provider<const Algorithm> ||
            config::configuration_provider<const SMSpec> ||
            config::configuration_provider<const NHESpec>)
    [[nodiscard]]
    auto configuration() const
    {
        auto children = std::tuple_cat(
            config::configuration_nodes(algorithm_),
            config::configuration_nodes(solution_manager_spec_),
            config::configuration_nodes(neighborhood_spec_));

        return std::apply(
            [](auto... nodes) {
                return config::named<Name>(std::move(nodes)...);
            },
            std::move(children));
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
        return detail::bound_runner<Algorithm, SMSpec, NHESpec>{
            algorithm_,
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
        return detail::bound_runner<Algorithm, SMSpec, NHESpec>{
            std::move(algorithm_),
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
    Algorithm algorithm_;
    SMSpec solution_manager_spec_;
    NHESpec neighborhood_spec_;
};

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

template<class Algorithm>
Runner(Algorithm) -> Runner<std::remove_cvref_t<Algorithm>>;

// Constructs a search algorithm from its arguments and wraps it in a Runner.
template<class Algorithm, class... Args>
    requires std::constructible_from<Algorithm, Args&&...>
[[nodiscard]]
auto make_runner(Args&&... args)
{
    return Runner<Algorithm>{Algorithm{std::forward<Args>(args)...}};
}

} // namespace easylocal
