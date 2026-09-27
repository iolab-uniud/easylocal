#pragma once

#include <easylocal/runner.hpp>
#include <easylocal/solver.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal
{

namespace detail
{

template<class Tag>
concept configurable_app_runner_tag =
    requires {
        typename Tag::config_type;
    } &&
    std::default_initializable<typename Tag::config_type> &&
    requires(typename Tag::config_type config) {
        Tag::make(std::move(config));
    };

template<configurable_app_runner_tag Tag>
struct app_runner_registration
{
    using tag_type = Tag;
    using config_type = typename Tag::config_type;
    using algorithm_type = decltype(Tag::make(std::declval<config_type>()));

    std::string name;
    config_type config{};
};

template<class Tag, class... Registrations>
inline constexpr std::size_t app_runner_count_v =
    (std::size_t{0} + ... +
     (std::same_as<Tag, typename Registrations::tag_type> ? 1U : 0U));

template<class Tag, std::size_t Index, class First, class... Rest>
consteval auto app_runner_index_impl() -> std::size_t
{
    if constexpr (std::same_as<Tag, typename First::tag_type>)
    {
        return Index;
    }
    else
    {
        static_assert(sizeof...(Rest) != 0, "runner tag is not registered in app");
        return app_runner_index_impl<Tag, Index + 1, Rest...>();
    }
}

template<class Tag, class... Registrations>
consteval auto app_runner_index() -> std::size_t
{
    static_assert(
        app_runner_count_v<Tag, Registrations...> == 1,
        "runner<Tag>() requires exactly one registration of Tag");
    return app_runner_index_impl<Tag, 0, Registrations...>();
}

template<class Tag, std::size_t Index = 0, class Tuple>
[[nodiscard]]
auto app_runner_registration_by_name(Tuple& registrations, const std::string_view name)
    -> app_runner_registration<Tag>&
{
    if constexpr (Index == std::tuple_size_v<std::remove_reference_t<Tuple>>)
    {
        throw std::invalid_argument{
            "runner '" + std::string{name} + "' is not registered for the requested tag"};
    }
    else
    {
        using registration_type = std::tuple_element_t<
            Index,
            std::remove_reference_t<Tuple>>;

        if constexpr (std::same_as<Tag, typename registration_type::tag_type>)
        {
            auto& registration = std::get<Index>(registrations);
            if (registration.name == name)
            {
                return registration;
            }
        }

        return app_runner_registration_by_name<Tag, Index + 1>(
            registrations,
            name);
    }
}

template<class Tag, std::size_t Index = 0, class Tuple>
[[nodiscard]]
auto app_runner_registration_by_name(
    const Tuple& registrations,
    const std::string_view name) -> const app_runner_registration<Tag>&
{
    if constexpr (Index == std::tuple_size_v<std::remove_reference_t<Tuple>>)
    {
        throw std::invalid_argument{
            "runner '" + std::string{name} + "' is not registered for the requested tag"};
    }
    else
    {
        using registration_type = std::tuple_element_t<
            Index,
            std::remove_reference_t<Tuple>>;

        if constexpr (std::same_as<Tag, typename registration_type::tag_type>)
        {
            const auto& registration = std::get<Index>(registrations);
            if (registration.name == name)
            {
                return registration;
            }
        }

        return app_runner_registration_by_name<Tag, Index + 1>(
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
    auto algorithm() noexcept -> Algorithm&
    {
        return algorithm_;
    }

    [[nodiscard]]
    auto initial_solution() const -> solution_type
        requires has_initial_solution<SM>
    {
        return solution_manager_.initial_solution();
    }

    template<class RNG>
    [[nodiscard]]
    auto random_solution(RNG& rng) const -> solution_type
        requires has_random_solution<SM, RNG>
    {
        return solution_manager_.random_solution(rng);
    }

    template<class... RunArgs>
    [[nodiscard]]
    auto run(solution_type solution, RunArgs&&... run_args)
        requires requires(
            Algorithm& algorithm,
            const runner_context<SM, NHE>& context,
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
            "initial Solution must be compatible with the app Input");

        const runner_context<SM, NHE> context{
            solution_manager_,
            neighborhood_,
        };

        return algorithm_.run(
            context,
            std::move(solution),
            std::forward<RunArgs>(run_args)...);
    }

private:
    Algorithm& algorithm_;
    SM& solution_manager_;
    NHE& neighborhood_;
};

template<class SMSpec, class NHESpec, class... Registrations>
    requires is_solution_manager_spec_v<SMSpec> &&
             is_neighborhood_spec_v<NHESpec> &&
             runner_neighborhood_explorer<service_t<NHESpec>, service_t<SMSpec>>
class app_instance
{
public:
    using solution_manager_type = service_t<SMSpec>;
    using neighborhood_explorer_type = service_t<NHESpec>;
    using instance_type = typename solution_manager_type::instance_type;

    static constexpr std::size_t runner_count = sizeof...(Registrations);

    app_instance(
        const instance_type& instance,
        const SMSpec& solution_manager_spec,
        const NHESpec& neighborhood_spec,
        const std::tuple<Registrations...>& registrations)
        : instance_{instance},
          solution_manager_{solution_manager_spec.construct(instance_)},
          neighborhood_{neighborhood_spec.construct(solution_manager_)},
          algorithms_{make_algorithms(registrations)}
    {
        assert(
            std::addressof(solution_manager_.instance()) ==
                std::addressof(instance_));
        assert(
            std::addressof(neighborhood_.instance()) ==
                std::addressof(instance_));
    }

    app_instance(const app_instance&) = delete;
    auto operator=(const app_instance&) -> app_instance& = delete;
    app_instance(app_instance&&) = delete;
    auto operator=(app_instance&&) -> app_instance& = delete;

    [[nodiscard]]
    auto instance() const noexcept -> const instance_type&
    {
        return instance_;
    }

    [[nodiscard]]
    auto solution_manager() noexcept -> solution_manager_type&
    {
        return solution_manager_;
    }

    [[nodiscard]]
    auto solution_manager() const noexcept -> const solution_manager_type&
    {
        return solution_manager_;
    }

    [[nodiscard]]
    auto neighborhood() noexcept -> neighborhood_explorer_type&
    {
        return neighborhood_;
    }

    [[nodiscard]]
    auto neighborhood() const noexcept -> const neighborhood_explorer_type&
    {
        return neighborhood_;
    }

    template<class Tag>
    [[nodiscard]]
    auto runner()
    {
        constexpr auto index = app_runner_index<Tag, Registrations...>();
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

    template<class Tag, class... RunArgs>
    [[nodiscard]]
    auto run(typename solution_manager_type::solution_type solution, RunArgs&&... args)
    {
        return runner<Tag>().run(
            std::move(solution),
            std::forward<RunArgs>(args)...);
    }

private:
    static auto make_algorithms(const std::tuple<Registrations...>& registrations)
    {
        return std::apply(
            [](const auto&... registration) {
                return std::tuple{
                    Registrations::tag_type::make(registration.config)...};
            },
            registrations);
    }

    const instance_type& instance_;
    solution_manager_type solution_manager_;
    neighborhood_explorer_type neighborhood_;
    std::tuple<typename Registrations::algorithm_type...> algorithms_;
};

template<class SMSpec, class NHESpec, class... Registrations>
class app_builder
{
public:
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
    auto name() const noexcept -> std::string_view
    {
        return name_;
    }

    template<class Spec>
        requires std::same_as<SMSpec, unconfigured_t> &&
                 is_solution_manager_spec_v<std::remove_cvref_t<Spec>>
    [[nodiscard]]
    auto solution_manager(Spec&& spec) &&
    {
        using spec_type = std::remove_cvref_t<Spec>;
        return app_builder<spec_type, NHESpec, Registrations...>{
            std::move(name_),
            std::forward<Spec>(spec),
            std::move(neighborhood_spec_),
            std::move(registrations_),
        };
    }

    template<class SM, class... Args>
        requires std::same_as<SMSpec, unconfigured_t>
    [[nodiscard]]
    auto solution_manager(Args&&... args) &&
    {
        return std::move(*this).solution_manager(
            easylocal::solution_manager<SM>(std::forward<Args>(args)...));
    }

    template<class Spec>
        requires (!std::same_as<SMSpec, unconfigured_t>) &&
                 std::same_as<NHESpec, unconfigured_t> &&
                 is_neighborhood_spec_v<std::remove_cvref_t<Spec>>
    [[nodiscard]]
    auto neighborhood(Spec&& spec) &&
    {
        using spec_type = std::remove_cvref_t<Spec>;
        return app_builder<SMSpec, spec_type, Registrations...>{
            std::move(name_),
            std::move(solution_manager_spec_),
            std::forward<Spec>(spec),
            std::move(registrations_),
        };
    }

    template<class NHE, class... Args>
        requires (!std::same_as<SMSpec, unconfigured_t>) &&
                 std::same_as<NHESpec, unconfigured_t>
    [[nodiscard]]
    auto neighborhood(Args&&... args) &&
    {
        return std::move(*this).neighborhood(
            easylocal::neighborhood<NHE>(std::forward<Args>(args)...));
    }

    template<configurable_app_runner_tag Tag>
        requires (!std::same_as<SMSpec, unconfigured_t>) &&
                 (!std::same_as<NHESpec, unconfigured_t>)
    [[nodiscard]]
    auto runner(std::string name) &&
    {
        using registration_type = app_runner_registration<Tag>;
        auto registrations = std::tuple_cat(
            std::move(registrations_),
            std::tuple{registration_type{.name = std::move(name)}});

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

    template<class Tag>
        requires (app_runner_count_v<Tag, Registrations...> == 1)
    [[nodiscard]]
    auto runner_config() noexcept -> typename Tag::config_type&
    {
        constexpr auto index = app_runner_index<Tag, Registrations...>();
        return std::get<index>(registrations_).config;
    }

    template<class Tag>
        requires (app_runner_count_v<Tag, Registrations...> == 1)
    [[nodiscard]]
    auto runner_config() const noexcept -> const typename Tag::config_type&
    {
        constexpr auto index = app_runner_index<Tag, Registrations...>();
        return std::get<index>(registrations_).config;
    }

    template<class Tag>
        requires (app_runner_count_v<Tag, Registrations...> > 0)
    [[nodiscard]]
    auto runner_config(const std::string_view name) -> typename Tag::config_type&
    {
        return app_runner_registration_by_name<Tag>(registrations_, name).config;
    }

    template<class Tag>
        requires (app_runner_count_v<Tag, Registrations...> > 0)
    [[nodiscard]]
    auto runner_config(const std::string_view name) const -> const typename Tag::config_type&
    {
        return app_runner_registration_by_name<Tag>(registrations_, name).config;
    }

    template<class Tag>
        requires (app_runner_count_v<Tag, Registrations...> == 1)
    [[nodiscard]]
    auto runner_name() const noexcept -> std::string_view
    {
        constexpr auto index = app_runner_index<Tag, Registrations...>();
        return std::get<index>(registrations_).name;
    }

    template<class Tag>
        requires (app_runner_count_v<Tag, Registrations...> == 1) &&
                 std::copy_constructible<SMSpec> &&
                 std::copy_constructible<NHESpec>
    [[nodiscard]]
    auto make_runner() const
    {
        constexpr auto index = app_runner_index<Tag, Registrations...>();
        const auto& registration = std::get<index>(registrations_);
        return Runner{Tag::make(registration.config)}
            | solution_manager_spec_
            | neighborhood_spec_;
    }

    template<class Tag>
        requires (app_runner_count_v<Tag, Registrations...> > 0) &&
                 std::copy_constructible<SMSpec> &&
                 std::copy_constructible<NHESpec>
    [[nodiscard]]
    auto make_runner(const std::string_view name) const
    {
        const auto& registration =
            app_runner_registration_by_name<Tag>(registrations_, name);
        return Runner{Tag::make(registration.config)}
            | solution_manager_spec_
            | neighborhood_spec_;
    }

    template<class SolverTag, class RunnerTag, class SolverConfig>
        requires (app_runner_count_v<RunnerTag, Registrations...> == 1) &&
                 std::copy_constructible<SMSpec> &&
                 std::copy_constructible<NHESpec> &&
                 solver_factory_tag<
                     SolverTag,
                     decltype(std::declval<const app_builder&>().template make_runner<RunnerTag>()),
                     SolverConfig>
    [[nodiscard]]
    auto make_solver(SolverConfig&& config) const
    {
        return easylocal::make_solver<SolverTag>(
            make_runner<RunnerTag>(),
            std::forward<SolverConfig>(config));
    }

    template<class SolverTag, class RunnerTag, class SolverConfig>
        requires (app_runner_count_v<RunnerTag, Registrations...> > 0) &&
                 std::copy_constructible<SMSpec> &&
                 std::copy_constructible<NHESpec> &&
                 solver_factory_tag<
                     SolverTag,
                     decltype(std::declval<const app_builder&>().template make_runner<RunnerTag>(std::declval<std::string_view>())),
                     SolverConfig>
    [[nodiscard]]
    auto make_solver(
        const std::string_view runner_name,
        SolverConfig&& config) const
    {
        return easylocal::make_solver<SolverTag>(
            make_runner<RunnerTag>(runner_name),
            std::forward<SolverConfig>(config));
    }

    template<class Spec = SMSpec>
        requires (!std::same_as<Spec, unconfigured_t>) &&
                 (!std::same_as<NHESpec, unconfigured_t>) &&
                 (sizeof...(Registrations) > 0) &&
                 Spec::template constructible_from<
                     const typename service_t<Spec>::instance_type> &&
                 NHESpec::template constructible_from<service_t<Spec>>
    [[nodiscard]]
    auto for_input(const typename service_t<Spec>::instance_type& instance) const
    {
        return app_instance<SMSpec, NHESpec, Registrations...>{
            instance,
            solution_manager_spec_,
            neighborhood_spec_,
            registrations_,
        };
    }

private:
    std::string name_;
    [[no_unique_address]] SMSpec solution_manager_spec_{};
    [[no_unique_address]] NHESpec neighborhood_spec_{};
    std::tuple<Registrations...> registrations_{};
};

} // namespace detail

[[nodiscard]]
inline auto app(std::string name)
{
    return detail::app_builder<
        detail::unconfigured_t,
        detail::unconfigured_t>{std::move(name)};
}

} // namespace easylocal
