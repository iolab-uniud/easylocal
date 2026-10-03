// The tabu customization points of a neighborhood: inverse, configurable on
// the explorer, and tabu_attribute, chosen by the explorer or the move itself;
// a neighborhood union dispatches both to its children.
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/neighborhood_union.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <iostream>
#include <optional>
#include <random>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>

namespace
{

struct Instance
{
};

struct Schedule
{
    std::array<int, 4> jobs{0, 1, 2, 3};
};

class ScheduleManager
{
public:
    using input_type = Instance;
    using solution_type = Schedule;

    explicit ScheduleManager(const Instance& instance) noexcept : instance_{instance} {}

    [[nodiscard]] auto input() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]] static auto is_valid(const Schedule&) noexcept -> bool
    {
        return true;
    }

private:
    const Instance& instance_;
};

// A swap of the jobs at two positions, with the jobs it moves.
struct Swap
{
    std::size_t first_position{};
    std::size_t second_position{};
    int first_job{};
    int second_job{};
};

struct JobPair
{
    int low{};
    int high{};

    friend auto operator==(const JobPair&, const JobPair&) -> bool = default;
};

} // namespace

template<>
struct std::hash<JobPair>
{
    auto operator()(const JobPair& pair) const noexcept -> std::size_t
    {
        return easylocal::hash_combine(easylocal::hash_combine(0, pair.low), pair.high);
    }
};

namespace
{

// The two inverse definitions of the tabu list study: IN1 forbids moving the
// same pair of jobs again, IN2 moving either job.
enum class InverseKind
{
    both_jobs,
    either_job,
};

class SwapExplorer
{
public:
    using input_type = Instance;
    using solution_type = Schedule;
    using move_type = Swap;

    explicit SwapExplorer(
        const ScheduleManager& manager,
        InverseKind kind = InverseKind::both_jobs)
        : instance_{manager.input()}, kind_{kind}
    {
    }

    [[nodiscard]] auto input() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]] static auto is_valid(const Schedule&, const Swap&) noexcept -> bool
    {
        return true;
    }

    static void make_move(Schedule& schedule, const Swap& swap)
    {
        std::swap(
            schedule.jobs[swap.first_position],
            schedule.jobs[swap.second_position]);
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const Schedule& schedule, RNG&)
        -> std::optional<Swap>
    {
        return Swap{0, 1, schedule.jobs[0], schedule.jobs[1]};
    }

    [[nodiscard]] auto inverse(const Schedule&, const Swap& move, const Swap& tabu_move)
        const -> bool
    {
        const auto moves = [&move](const int job) {
            return move.first_job == job || move.second_job == job;
        };
        if (kind_ == InverseKind::both_jobs)
            return moves(tabu_move.first_job) && moves(tabu_move.second_job);
        return moves(tabu_move.first_job) || moves(tabu_move.second_job);
    }

    [[nodiscard]] static auto tabu_attribute(const Swap& move) -> JobPair
    {
        return {
            .low = std::min(move.first_job, move.second_job),
            .high = std::max(move.first_job, move.second_job)};
    }

private:
    const Instance& instance_;
    InverseKind kind_;
};

// A move of one job to the end, without tabu members; the move is hashable,
// so it is its own attribute.
class RotateExplorer
{
public:
    using input_type = Instance;
    using solution_type = Schedule;
    using move_type = int;

    explicit RotateExplorer(const ScheduleManager& manager) : instance_{manager.input()}
    {
    }

    [[nodiscard]] auto input() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]] static auto is_valid(const Schedule&, int) noexcept -> bool
    {
        return true;
    }

    static void make_move(Schedule&, int) {}

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]] static auto random_move(const Schedule&, RNG&) -> std::optional<int>
    {
        return 0;
    }

private:
    const Instance& instance_;
};

// RotateExplorer with an inverse: a job moved to the end may not move again.
class TabuRotateExplorer : public RotateExplorer
{
public:
    using RotateExplorer::RotateExplorer;

    [[nodiscard]] static auto inverse(const Schedule&, int move, int tabu_move) -> bool
    {
        return move == tabu_move;
    }
};

static_assert(easylocal::inverse_neighborhood_for<SwapExplorer, Schedule>);
static_assert(easylocal::has_tabu_attribute_member<SwapExplorer>);
static_assert(std::same_as<easylocal::tabu_attribute_t<SwapExplorer>, JobPair>);

static_assert(!easylocal::inverse_neighborhood_for<RotateExplorer, Schedule>);
static_assert(easylocal::has_tabu_attribute<RotateExplorer>);
static_assert(!easylocal::has_tabu_attribute_member<RotateExplorer>);
static_assert(std::same_as<easylocal::tabu_attribute_t<RotateExplorer>, int>);

using TabuUnion =
    easylocal::detail::neighborhood_union_explorer<SwapExplorer, TabuRotateExplorer>;
using PartialUnion =
    easylocal::detail::neighborhood_union_explorer<SwapExplorer, RotateExplorer>;
using TwinUnion =
    easylocal::detail::neighborhood_union_explorer<RotateExplorer, TabuRotateExplorer>;

static_assert(easylocal::inverse_neighborhood_for<TabuUnion, Schedule>);
static_assert(
    !easylocal::inverse_neighborhood_for<PartialUnion, Schedule>,
    "a union has an inverse only when every child has one");
static_assert(easylocal::has_tabu_attribute<PartialUnion>);
static_assert(easylocal::has_tabu_attribute<TwinUnion>);

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
    const ScheduleManager manager{instance};
    const Schedule schedule;

    // The tabu move swapped jobs 1 and 0; the candidates swap 0 and 1 again,
    // or 0 and 2.
    const Swap tabu{0, 2, 1, 0};
    const Swap same_jobs{1, 3, 0, 1};
    const Swap one_job{0, 3, 0, 2};

    const SwapExplorer both{manager, InverseKind::both_jobs};
    const SwapExplorer either{manager, InverseKind::either_job};
    ok &= expect(
        easylocal::inverse(both, schedule, same_jobs, tabu)
            && !easylocal::inverse(both, schedule, one_job, tabu),
        "IN1 forbids moving the same pair of jobs again");
    ok &= expect(
        easylocal::inverse(either, schedule, same_jobs, tabu)
            && easylocal::inverse(either, schedule, one_job, tabu),
        "IN2 forbids moving either job again");

    ok &= expect(
        easylocal::tabu_attribute(both, same_jobs)
                == easylocal::tabu_attribute(both, tabu)
            && easylocal::tabu_attribute(both, one_job)
                != easylocal::tabu_attribute(both, tabu),
        "the explorer's attribute ignores the positions");
    const RotateExplorer rotate{manager};
    ok &= expect(
        easylocal::tabu_attribute(rotate, 2) == 2,
        "a hashable move is its own attribute");

    const TabuUnion
        tabu_union{{1.0, 1.0}, SwapExplorer{manager}, TabuRotateExplorer{manager}};
    using union_move = TabuUnion::move_type;
    const auto swap_move = [](const Swap& swap) {
        return union_move{
            std::in_place_index<0>,
            std::variant_alternative_t<0, union_move>{swap}};
    };
    const auto rotate_move = [](const int job) {
        return union_move{
            std::in_place_index<1>,
            std::variant_alternative_t<1, union_move>{job}};
    };
    ok &= expect(
        easylocal::inverse(tabu_union, schedule, swap_move(same_jobs), swap_move(tabu))
            && !easylocal::inverse(
                tabu_union,
                schedule,
                swap_move(one_job),
                swap_move(tabu)),
        "a union delegates the inverse to the child of both moves");
    ok &= expect(
        easylocal::inverse(tabu_union, schedule, rotate_move(1), rotate_move(1))
            && !easylocal::inverse(tabu_union, schedule, rotate_move(1), swap_move(tabu)),
        "moves of different children never forbid each other");

    // Equal attributes (both int) of two children are different union attributes.
    const TwinUnion
        twins{{1.0, 1.0}, RotateExplorer{manager}, TabuRotateExplorer{manager}};
    using twin_move = TwinUnion::move_type;
    const auto twin = [](auto index, const int job) {
        constexpr auto alternative = decltype(index)::value;
        return twin_move{
            std::in_place_index<alternative>,
            std::variant_alternative_t<alternative, twin_move>{job}};
    };
    const auto first = easylocal::tabu_attribute(
        twins,
        twin(std::integral_constant<std::size_t, 0>{}, 3));
    const auto second = easylocal::tabu_attribute(
        twins,
        twin(std::integral_constant<std::size_t, 1>{}, 3));
    ok &= expect(
        first != second
            && first
                == easylocal::tabu_attribute(
                    twins,
                    twin(std::integral_constant<std::size_t, 0>{}, 3)),
        "a union tags the attribute with its child");
    std::unordered_set<easylocal::tabu_attribute_t<TwinUnion>> seen{first, second};
    ok &= expect(seen.size() == 2, "union attributes are hashable");

    return ok ? 0 : 1;
}
