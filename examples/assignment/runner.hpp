#pragma once

#include <cassert>
#include <compare>
#include <concepts>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

namespace easylocal::mwe::assignment
{

namespace detail
{

struct unbound_t
{
};

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

} // namespace detail

template<class SM, class NHE>
    requires detail::runner_neighborhood_explorer<NHE, SM>
class RunnerContext
{
public:
    using instance_type = typename SM::instance_type;
    using solution_type = typename SM::solution_type;
    using cost_type = typename SM::cost_type;
    using solution_manager_type = SM;
    using neighborhood_explorer_type = NHE;

    RunnerContext(const SM& solution_manager, const NHE& neighborhood) noexcept
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

template<
    class Algorithm,
    class SM = detail::unbound_t,
    class NHE = detail::unbound_t>
class Runner;

template<class Algorithm>
class Runner<Algorithm, detail::unbound_t, detail::unbound_t>
{
public:
    explicit Runner(Algorithm algorithm)
        : algorithm_{std::move(algorithm)}
    {
    }

    template<detail::runner_solution_manager SM>
        requires std::copy_constructible<Algorithm>
    [[nodiscard]]
    auto bind(const SM& solution_manager) const & -> Runner<Algorithm, SM>
    {
        return Runner<Algorithm, SM>{algorithm_, solution_manager};
    }

    template<detail::runner_solution_manager SM>
    [[nodiscard]]
    auto bind(const SM& solution_manager) && -> Runner<Algorithm, SM>
    {
        return Runner<Algorithm, SM>{
            std::move(algorithm_),
            solution_manager,
        };
    }

private:
    Algorithm algorithm_;
};

template<class Algorithm, detail::runner_solution_manager SM>
class Runner<Algorithm, SM, detail::unbound_t>
{
public:
    Runner(Algorithm algorithm, const SM& solution_manager)
        : algorithm_{std::move(algorithm)}, solution_manager_{solution_manager}
    {
    }

    template<class NHE>
        requires detail::runner_neighborhood_explorer<NHE, SM> &&
                 std::copy_constructible<Algorithm>
    [[nodiscard]]
    auto bind(const NHE& neighborhood) const & -> Runner<Algorithm, SM, NHE>
    {
        assert_same_instance(neighborhood);
        return Runner<Algorithm, SM, NHE>{
            algorithm_,
            solution_manager_,
            neighborhood,
        };
    }

    template<class NHE>
        requires detail::runner_neighborhood_explorer<NHE, SM>
    [[nodiscard]]
    auto bind(const NHE& neighborhood) && -> Runner<Algorithm, SM, NHE>
    {
        assert_same_instance(neighborhood);
        return Runner<Algorithm, SM, NHE>{
            std::move(algorithm_),
            solution_manager_,
            neighborhood,
        };
    }

private:
    template<class NHE>
    void assert_same_instance(const NHE& neighborhood) const noexcept
    {
        assert(
            std::addressof(solution_manager_.instance()) ==
                std::addressof(neighborhood.instance()) &&
            "SolutionManager and NeighborhoodExplorer must share an Instance");
    }

    Algorithm algorithm_;
    const SM& solution_manager_;
};

template<
    class Algorithm,
    detail::runner_solution_manager SM,
    class NHE>
    requires detail::runner_neighborhood_explorer<NHE, SM>
class Runner<Algorithm, SM, NHE>
{
public:
    using instance_type = typename SM::instance_type;
    using solution_type = typename SM::solution_type;

    Runner(
        Algorithm algorithm,
        const SM& solution_manager,
        const NHE& neighborhood) noexcept(
            std::is_nothrow_move_constructible_v<Algorithm>)
        : algorithm_{std::move(algorithm)},
          solution_manager_{solution_manager},
          neighborhood_{neighborhood}
    {
    }

    [[nodiscard]]
    auto run(const instance_type& instance, solution_type solution)
        requires requires(
            Algorithm& algorithm,
            const RunnerContext<SM, NHE>& context,
            solution_type candidate)
        {
            algorithm.run(context, std::move(candidate));
        }
    {
        assert(
            std::addressof(instance) ==
                std::addressof(solution_manager_.instance()) &&
            "run() Instance must match the bound SolutionManager");
        assert(
            std::addressof(instance) ==
                std::addressof(neighborhood_.instance()) &&
            "run() Instance must match the bound NeighborhoodExplorer");
        assert(
            solution_manager_.is_valid(solution) &&
            "initial Solution must be compatible with the run Instance");

        const RunnerContext<SM, NHE> context{
            solution_manager_,
            neighborhood_,
        };

        return algorithm_.run(context, std::move(solution));
    }

private:
    Algorithm algorithm_;
    const SM& solution_manager_;
    const NHE& neighborhood_;
};

template<class Algorithm>
Runner(Algorithm) -> Runner<std::remove_cvref_t<Algorithm>>;

} // namespace easylocal::mwe::assignment
