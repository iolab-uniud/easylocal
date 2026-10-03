// Optional solution identity: a hash and an equality from the SolutionManager
// or from the solution type, and the helpers to write a hash.
#include <easylocal/cost.hpp>
#include <easylocal/helpers/detail/cost_layer.hpp>
#include <easylocal/helpers/recipes.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/utils/hash.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <vector>

namespace
{

struct Instance
{
};

// A permutation with a cache that does not identify it.
struct Tour
{
    std::vector<int> order;
    int cached_length{};

    friend auto operator==(const Tour&, const Tour&) -> bool = default;
};

struct Opaque
{
    int value{};
};

} // namespace

template<>
struct std::hash<Opaque>
{
    auto operator()(const Opaque& opaque) const noexcept -> std::size_t
    {
        return std::hash<int>{}(opaque.value);
    }
};

namespace
{

// The SolutionManager identifies a tour by its order alone, and a tour with
// its reverse.
class TourManager : public easylocal::solution_manager_base<Instance, Tour>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]] static auto is_valid(const Tour&) noexcept -> bool
    {
        return true;
    }

    [[nodiscard]] static auto hash(const Tour& tour) -> std::uint64_t
    {
        const auto forward = easylocal::hash_range(tour.order);
        const auto backward = easylocal::hash_range(tour.order | std::views::reverse);
        return forward ^ backward;
    }

    [[nodiscard]] static auto equal(const Tour& lhs, const Tour& rhs) -> bool
    {
        return lhs.order == rhs.order
            || std::ranges::equal(lhs.order, rhs.order | std::views::reverse);
    }
};

struct CachedLength
{
    [[nodiscard]] static auto evaluate(const Tour& tour) noexcept -> int
    {
        return tour.cached_length;
    }
};

struct TourSize
{
    [[nodiscard]] static auto evaluate(const Tour& tour) noexcept -> int
    {
        return static_cast<int>(tour.order.size());
    }
};

// No members: the identity comes from the solution type, if it has one.
template<class Solution>
class PlainManager : public easylocal::solution_manager_base<Instance, Solution>
{
public:
    using easylocal::solution_manager_base<Instance, Solution>::solution_manager_base;

    [[nodiscard]] static auto is_valid(const Solution&) noexcept -> bool
    {
        return true;
    }
};

static_assert(easylocal::has_solution_hash<TourManager>);
static_assert(easylocal::has_solution_equality<TourManager>);
static_assert(easylocal::has_solution_hash<PlainManager<Opaque>>);
static_assert(!easylocal::has_solution_equality<PlainManager<Opaque>>);
static_assert(!easylocal::has_solution_hash<PlainManager<Tour>>);
static_assert(easylocal::has_solution_equality<PlainManager<Tour>>);
static_assert(easylocal::has_solution_hash<PlainManager<int>>);

auto expect(const bool condition, const std::string_view description) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }
    return true;
}

} // namespace

int main()
{
    bool ok = true;
    const Instance instance;

    const TourManager tours{instance};
    const Tour tour{.order = {0, 1, 2, 3}, .cached_length = 10};
    const Tour stale{.order = {0, 1, 2, 3}, .cached_length = 99};
    const Tour reversed{.order = {3, 2, 1, 0}, .cached_length = 10};
    const Tour other{.order = {0, 2, 1, 3}, .cached_length = 10};

    ok &= expect(
        easylocal::solution_hash(tours, tour) == easylocal::solution_hash(tours, stale)
            && easylocal::solutions_equal(tours, tour, stale),
        "the SolutionManager's identity leaves out the cache");
    ok &= expect(
        easylocal::solution_hash(tours, tour) == easylocal::solution_hash(tours, reversed)
            && easylocal::solutions_equal(tours, tour, reversed),
        "the SolutionManager's identity takes precedence over operator==");
    ok &= expect(
        easylocal::solution_hash(tours, tour) != easylocal::solution_hash(tours, other)
            && !easylocal::solutions_equal(tours, tour, other),
        "different tours are told apart");

    ok &= expect(
        easylocal::hash_combine(0, 1) != easylocal::hash_combine(0, 2)
            && easylocal::hash_range(std::array{1, 2})
                != easylocal::hash_range(std::array{2, 1}),
        "hash_combine tells values apart and hash_range depends on the order");

    // A SolutionManager in a recipe, with its cost layers, keeps its identity.
    const auto recipe =
        easylocal::solution_manager<TourManager>() | easylocal::component<CachedLength>();
    const auto service = recipe.construct(instance);
    using service_type = std::remove_const_t<decltype(service)>;
    static_assert(easylocal::has_solution_hash_member<service_type>);
    static_assert(easylocal::has_solution_equality_member<service_type>);
    const auto hard_soft_recipe = easylocal::solution_manager<TourManager>()
        | easylocal::cost::hard_soft(
            easylocal::component<TourSize>(),
            easylocal::component<CachedLength>());
    using hard_soft_type = decltype(hard_soft_recipe.construct(instance));
    const easylocal::detail::hard_cost_layer<hard_soft_type> hard{
        hard_soft_recipe.construct(instance)};
    static_assert(easylocal::has_solution_hash_member<decltype(hard)>);
    ok &= expect(
        easylocal::solutions_equal(service, tour, reversed)
            && easylocal::solution_hash(service, tour)
                == easylocal::solution_hash(tours, tour)
            && easylocal::solutions_equal(hard, tour, reversed),
        "the cost layers forward the SolutionManager's identity");

    const auto plain_recipe = easylocal::solution_manager<PlainManager<Tour>>()
        | easylocal::component<CachedLength>();
    const auto plain_service = plain_recipe.construct(instance);
    static_assert(!easylocal::has_solution_equality_member<
        std::remove_const_t<decltype(plain_service)>>);
    ok &= expect(
        !easylocal::solutions_equal(plain_service, tour, stale),
        "without members the cost layers leave the solution type's identity");

    const PlainManager<Opaque> opaques{instance};
    ok &= expect(
        easylocal::solution_hash(opaques, Opaque{.value = 4})
            == std::hash<Opaque>{}(Opaque{.value = 4}),
        "without a member the hash is the solution type's std::hash");

    const PlainManager<Tour> plain{instance};
    ok &= expect(
        !easylocal::solutions_equal(plain, tour, stale),
        "without a member the equality is the solution type's operator==");

    return ok ? 0 : 1;
}
