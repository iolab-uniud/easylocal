#include "support/expect.hpp"

#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/runners/runner.hpp>

#include <cstdint>
#include <random>

namespace
{

struct Instance
{
    int deterministic_value{7};
};

struct Solution
{
    int value{};

    friend constexpr auto operator==(const Solution&, const Solution&) -> bool = default;
};

class MinimalSolutionManager
{
public:
    using input_type = Instance;
    using solution_type = Solution;

    explicit MinimalSolutionManager(const Instance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto input() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]]
    static auto is_valid(const Solution&) noexcept -> bool
    {
        return true;
    }

private:
    const Instance& instance_;
};

class DeterministicSolutionManager : public MinimalSolutionManager
{
public:
    using MinimalSolutionManager::MinimalSolutionManager;

    [[nodiscard]]
    auto initial_solution() const noexcept -> Solution
    {
        return Solution{.value = input().deterministic_value};
    }
};

class RandomSolutionManager : public MinimalSolutionManager
{
public:
    using MinimalSolutionManager::MinimalSolutionManager;

    template<class RNG>
    [[nodiscard]]
    auto random_solution(RNG& rng) const -> Solution
    {
        std::uniform_int_distribution<int> distribution{0, 1'000'000};
        return Solution{.value = distribution(rng)};
    }
};

class BothSolutionManager : public MinimalSolutionManager
{
public:
    using MinimalSolutionManager::MinimalSolutionManager;

    [[nodiscard]]
    auto initial_solution() const noexcept -> Solution
    {
        return Solution{.value = input().deterministic_value};
    }

    template<class RNG>
    [[nodiscard]]
    auto random_solution(RNG& rng) const -> Solution
    {
        std::uniform_int_distribution<int> distribution{0, 1'000'000};
        return Solution{.value = distribution(rng)};
    }
};

class WrongInitialReturnSolutionManager : public MinimalSolutionManager
{
public:
    using MinimalSolutionManager::MinimalSolutionManager;

    [[nodiscard]]
    auto initial_solution() const noexcept -> int
    {
        return 0;
    }
};

class WrongRandomReturnSolutionManager : public MinimalSolutionManager
{
public:
    using MinimalSolutionManager::MinimalSolutionManager;

    template<class RNG>
    [[nodiscard]]
    auto random_solution(RNG&) const noexcept -> int
    {
        return 0;
    }
};

class MutableOnlyInitialSolutionManager : public MinimalSolutionManager
{
public:
    using MinimalSolutionManager::MinimalSolutionManager;

    [[nodiscard]]
    auto initial_solution() noexcept -> Solution
    {
        return {};
    }
};

class MutableOnlyRandomSolutionManager : public MinimalSolutionManager
{
public:
    using MinimalSolutionManager::MinimalSolutionManager;

    template<class RNG>
    [[nodiscard]]
    auto random_solution(RNG&) noexcept -> Solution
    {
        return {};
    }
};

class ValueComponent
{
public:
    using value_type = int;

    explicit ValueComponent(const Instance&) noexcept
    {
    }

    [[nodiscard]]
    static auto evaluate(const Solution& solution) noexcept -> int
    {
        return solution.value;
    }
};

} // namespace

int main()
{
    using easylocal::component;
    using easylocal::solution_manager;
    using easylocal::has_initial_solution;
    using easylocal::has_random_solution;

    static_assert(!has_initial_solution<MinimalSolutionManager>);
    static_assert(!has_random_solution<MinimalSolutionManager, std::mt19937>);

    static_assert(has_initial_solution<DeterministicSolutionManager>);
    static_assert(!has_random_solution<DeterministicSolutionManager, std::mt19937>);

    static_assert(!has_initial_solution<RandomSolutionManager>);
    static_assert(has_random_solution<RandomSolutionManager, std::mt19937>);

    static_assert(has_initial_solution<BothSolutionManager>);
    static_assert(has_random_solution<BothSolutionManager, std::mt19937>);

    static_assert(!has_initial_solution<WrongInitialReturnSolutionManager>);
    static_assert(!has_random_solution<WrongRandomReturnSolutionManager, std::mt19937>);
    static_assert(!has_initial_solution<MutableOnlyInitialSolutionManager>);
    static_assert(!has_random_solution<MutableOnlyRandomSolutionManager, std::mt19937>);

    using ConfiguredRecipe = decltype(
        solution_manager<BothSolutionManager>() | component<ValueComponent>());
    using ConfiguredManager = typename ConfiguredRecipe::service_type;

    static_assert(has_initial_solution<ConfiguredManager>);
    static_assert(has_random_solution<ConfiguredManager, std::mt19937>);

    const Instance instance{.deterministic_value = 42};
    const auto configured =
        (solution_manager<BothSolutionManager>() | component<ValueComponent>())
            .construct(instance);

    bool ok = true;
    ok &= expect(
        configured.initial_solution() == Solution{.value = 42},
        "configured SolutionManager forwards initial_solution()");

    std::mt19937 direct_rng{0x5eedu};
    std::mt19937 configured_rng{0x5eedu};
    const BothSolutionManager direct{instance};

    const auto expected_random = direct.random_solution(direct_rng);
    const auto actual_random = configured.random_solution(configured_rng);

    ok &= expect(
        actual_random == expected_random,
        "configured SolutionManager forwards random_solution(rng) without changing RNG semantics");
    ok &= expect(
        configured_rng() == direct_rng(),
        "configured SolutionManager advances the caller-owned RNG exactly like the base manager");

    using ConfiguredDeterministicRecipe = decltype(
        solution_manager<DeterministicSolutionManager>() | component<ValueComponent>());
    using ConfiguredDeterministicManager =
        typename ConfiguredDeterministicRecipe::service_type;
    static_assert(has_initial_solution<ConfiguredDeterministicManager>);
    static_assert(!has_random_solution<ConfiguredDeterministicManager, std::mt19937>);

    return ok ? 0 : 1;
}
