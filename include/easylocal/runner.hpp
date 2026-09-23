#pragma once

#include <cassert>
#include <compare>
#include <concepts>
#include <memory>
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

struct solution_manager_tag
{
};

struct neighborhood_tag
{
};

template<class Tag, class Service, class... StoredArgs>
class service_spec
{
public:
    using tag_type = Tag;
    using service_type = Service;

    explicit service_spec(StoredArgs... args)
        : args_{std::move(args)...}
    {
    }

    template<class Dependency>
    static constexpr bool constructible_from =
        std::constructible_from<
            Service,
            Dependency&,
            const StoredArgs&...>;

    template<class Dependency>
        requires constructible_from<Dependency>
    [[nodiscard]]
    auto construct(Dependency& dependency) const -> Service
    {
        return std::apply(
            [&](const auto&... args) {
                return Service{dependency, args...};
            },
            args_);
    }

    [[nodiscard]]
    auto args() && noexcept -> std::tuple<StoredArgs...>&&
    {
        return std::move(args_);
    }

private:
    std::tuple<StoredArgs...> args_;
};

template<class T>
struct is_solution_manager_spec : std::false_type
{
};

template<class Service, class... Args>
struct is_solution_manager_spec<
    service_spec<solution_manager_tag, Service, Args...>> : std::true_type
{
};

template<class T>
inline constexpr bool is_solution_manager_spec_v =
    is_solution_manager_spec<T>::value;

template<class T>
struct is_neighborhood_spec : std::false_type
{
};

template<class Service, class... Args>
struct is_neighborhood_spec<
    service_spec<neighborhood_tag, Service, Args...>> : std::true_type
{
};

template<class T>
inline constexpr bool is_neighborhood_spec_v =
    is_neighborhood_spec<T>::value;

template<class SM>
concept runner_solution_manager =
    requires(const SM& solution_manager, const typename SM::solution_type& solution) {
        typename SM::instance_type;
        typename SM::solution_type;
        typename SM::cost_type;

        requires std::three_way_comparable<typename SM::cost_type>;

        {
            solution_manager.instance()
        } -> std::same_as<const typename SM::instance_type&>;

        {
            solution_manager.is_valid(solution)
        } -> std::convertible_to<bool>;

        {
            solution_manager.evaluate(solution)
        } -> std::same_as<typename SM::cost_type>;
    };

template<class NHE, class SM>
concept runner_neighborhood_explorer =
    runner_solution_manager<SM> &&
    requires(const NHE& neighborhood, const typename SM::solution_type& solution) {
        typename NHE::instance_type;
        typename NHE::solution_type;

        requires std::same_as<
            typename NHE::instance_type,
            typename SM::instance_type>;
        requires std::same_as<
            typename NHE::solution_type,
            typename SM::solution_type>;

        {
            neighborhood.instance()
        } -> std::same_as<const typename NHE::instance_type&>;

        { neighborhood.moves(solution) } -> std::ranges::input_range;
    };

template<class Spec>
using service_t = typename Spec::service_type;

template<class SM, class NHE>
    requires runner_neighborhood_explorer<NHE, SM>
class runner_context
{
public:
    using instance_type = typename SM::instance_type;
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

private:
    const SM& solution_manager_;
    const NHE& neighborhood_;
};

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
    using instance_type = typename solution_manager_type::instance_type;
    using solution_type = typename solution_manager_type::solution_type;

    bound_runner(
        Algorithm algorithm,
        const instance_type& instance,
        const SMSpec& solution_manager_spec,
        const NHESpec& neighborhood_spec)
        : algorithm_{std::move(algorithm)},
          instance_{instance},
          solution_manager_{solution_manager_spec.construct(instance_)},
          neighborhood_{neighborhood_spec.construct(solution_manager_)}
    {
        assert(
            std::addressof(solution_manager_.instance()) ==
                std::addressof(instance_) &&
            "SolutionManager must bind to the requested Instance");
        assert(
            std::addressof(neighborhood_.instance()) ==
                std::addressof(instance_) &&
            "NeighborhoodExplorer must share the bound Instance");
    }

    bound_runner(const bound_runner&) = delete;
    auto operator=(const bound_runner&) -> bound_runner& = delete;
    bound_runner(bound_runner&&) = delete;
    auto operator=(bound_runner&&) -> bound_runner& = delete;

    template<class... RunArgs>
    [[nodiscard]]
    auto run(solution_type solution, RunArgs&&... run_args)
        requires requires(
            Algorithm& algorithm,
            const runner_context<
                solution_manager_type,
                neighborhood_explorer_type>& context,
            solution_type candidate,
            RunArgs&&... forwarded_args)
        {
            algorithm.run(
                context,
                std::move(candidate),
                std::forward<RunArgs>(forwarded_args)...);
        }
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

        return algorithm_.run(
            context,
            std::move(solution),
            std::forward<RunArgs>(run_args)...);
    }

private:
    Algorithm algorithm_;
    const instance_type& instance_;
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

    template<detail::runner_solution_manager SM, class... Args>
        requires std::copy_constructible<Algorithm>
    [[nodiscard]]
    auto with_solution_manager(Args&&... args) const &
    {
        using spec_type = detail::service_spec<
            detail::solution_manager_tag,
            SM,
            std::decay_t<Args>...>;

        return Runner<Algorithm, spec_type>{
            algorithm_,
            spec_type{std::forward<Args>(args)...},
        };
    }

    template<detail::runner_solution_manager SM, class... Args>
    [[nodiscard]]
    auto with_solution_manager(Args&&... args) &&
    {
        using spec_type = detail::service_spec<
            detail::solution_manager_tag,
            SM,
            std::decay_t<Args>...>;

        return Runner<Algorithm, spec_type>{
            std::move(algorithm_),
            spec_type{std::forward<Args>(args)...},
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

    template<class NHE, class... Args>
        requires detail::runner_neighborhood_explorer<
                     NHE,
                     solution_manager_type> &&
                 std::copy_constructible<Algorithm> &&
                 std::copy_constructible<SMSpec>
    [[nodiscard]]
    auto with_neighborhood(Args&&... args) const &
    {
        using spec_type = detail::service_spec<
            detail::neighborhood_tag,
            NHE,
            std::decay_t<Args>...>;

        return with_neighborhood(
            spec_type{std::forward<Args>(args)...});
    }

    template<class NHE, class... Args>
        requires detail::runner_neighborhood_explorer<
            NHE,
            solution_manager_type>
    [[nodiscard]]
    auto with_neighborhood(Args&&... args) &&
    {
        using spec_type = detail::service_spec<
            detail::neighborhood_tag,
            NHE,
            std::decay_t<Args>...>;

        return std::move(*this).with_neighborhood(
            spec_type{std::forward<Args>(args)...});
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
    using instance_type = typename solution_manager_type::instance_type;

    Runner(
        Algorithm algorithm,
        SMSpec solution_manager_spec,
        NHESpec neighborhood_spec)
        : algorithm_{std::move(algorithm)},
          solution_manager_spec_{std::move(solution_manager_spec)},
          neighborhood_spec_{std::move(neighborhood_spec)}
    {
    }

    [[nodiscard]]
    auto bind(const instance_type& instance) const &
        requires std::copy_constructible<Algorithm> &&
                 (SMSpec::template constructible_from<const instance_type>) &&
                 (NHESpec::template constructible_from<solution_manager_type>)
    {
        return detail::bound_runner<Algorithm, SMSpec, NHESpec>{
            algorithm_,
            instance,
            solution_manager_spec_,
            neighborhood_spec_,
        };
    }

    [[nodiscard]]
    auto bind(const instance_type& instance) &&
        requires (SMSpec::template constructible_from<const instance_type>) &&
                 (NHESpec::template constructible_from<solution_manager_type>)
    {
        return detail::bound_runner<Algorithm, SMSpec, NHESpec>{
            std::move(algorithm_),
            instance,
            solution_manager_spec_,
            neighborhood_spec_,
        };
    }

private:
    Algorithm algorithm_;
    SMSpec solution_manager_spec_;
    NHESpec neighborhood_spec_;
};

template<class SM, class... Args>
[[nodiscard]]
auto solution_manager(Args&&... args)
{
    return detail::service_spec<
        detail::solution_manager_tag,
        SM,
        std::decay_t<Args>...>{std::forward<Args>(args)...};
}

template<class NHE, class... Args>
[[nodiscard]]
auto neighborhood(Args&&... args)
{
    return detail::service_spec<
        detail::neighborhood_tag,
        NHE,
        std::decay_t<Args>...>{std::forward<Args>(args)...};
}

template<class Algorithm, class SMSpec, class NHESpec, class SM, class... Args>
    requires std::same_as<SMSpec, detail::unconfigured_t> &&
             std::same_as<NHESpec, detail::unconfigured_t>
[[nodiscard]]
auto operator|(
    Runner<Algorithm, SMSpec, NHESpec> runner,
    detail::service_spec<detail::solution_manager_tag, SM, Args...> spec)
{
    return std::apply(
        [&]<class... StoredArgs>(StoredArgs&&... args) {
            return std::move(runner).template with_solution_manager<SM>(
                std::forward<StoredArgs>(args)...);
        },
        std::move(spec).args());
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

} // namespace easylocal
