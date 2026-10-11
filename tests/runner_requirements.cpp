// The requirements an algorithm declares agree with what its run() asks: on
// services that provide every declared capability the algorithm runs, and on
// services that lack one it does not, so a registration never names a
// capability the algorithm does not need.
#include <easylocal/app/app.hpp>
#include <easylocal/runners/best_improvement.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/runners/great_deluge.hpp>
#include <easylocal/runners/hill_climbing.hpp>
#include <easylocal/runners/late_acceptance_hill_climbing.hpp>
#include <easylocal/runners/pareto_late_acceptance_hill_climbing.hpp>
#include <easylocal/runners/requirements.hpp>
#include <easylocal/runners/simulated_annealing.hpp>
#include <easylocal/runners/tabu_search.hpp>

#include <compare>
#include <cstddef>
#include <optional>
#include <random>
#include <type_traits>
#include <vector>

namespace
{

namespace el = easylocal;
namespace needs = el::runners::needs;

struct Instance
{
    std::size_t size{4};
};

struct Solution
{
    int score{};
};

struct Move
{
    int delta{1};
};

// A cost of the three relations: better, equivalent and better_or_equivalent.
class ScoreComponent : public el::input_base<Instance>
{
public:
    using input_base::input_base;
    using value_type = int;

    [[nodiscard]]
    int evaluate(const Solution& solution) const
    {
        return solution.score;
    }
};

// A second objective, for the model of a cost::pareto cost.
class PenaltyComponent : public el::input_base<Instance>
{
public:
    using input_base::input_base;
    using value_type = int;

    [[nodiscard]]
    int evaluate(const Solution& solution) const
    {
        return -solution.score;
    }
};

// A cost with `<` alone: better, and no better_or_equivalent.
struct StrictOnly
{
    int value{};

    [[nodiscard]]
    friend bool operator<(const StrictOnly& lhs, const StrictOnly& rhs)
    {
        return lhs.value < rhs.value;
    }
};

struct StrictOnlyCost
{
    [[nodiscard]]
    StrictOnly operator()(const int value) const
    {
        return StrictOnly{value};
    }
};

// A cost of no relation at all: neither better nor better_or_equivalent.
struct Opaque
{
    int value{};
};

struct OpaqueCost
{
    [[nodiscard]]
    Opaque operator()(const int value) const
    {
        return Opaque{value};
    }
};

// The SolutionManager of the model, with a random solution.
class RandomManager : public el::solution_manager_base<Instance, Solution>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]]
    Solution initial_solution() const
    {
        return Solution{};
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    Solution random_solution(RNG& rng) const
    {
        return Solution{static_cast<int>(rng() % 100U)};
    }

    [[nodiscard]]
    bool is_valid(const Solution&) const
    {
        return true;
    }
};

// The same without random_solution().
class InitialOnlyManager : public el::solution_manager_base<Instance, Solution>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]]
    Solution initial_solution() const
    {
        return Solution{};
    }

    [[nodiscard]]
    bool is_valid(const Solution&) const
    {
        return true;
    }
};

// The moves of the model: enumerated, drawn at random, or both. Each explorer
// has the inverse the tabu lists use, so that only the capability under test
// changes.
template<class Manager, bool Enumerates, bool Draws>
class Explorer : public el::neighborhood_explorer_base<Manager, Move>
{
public:
    using el::neighborhood_explorer_base<Manager, Move>::neighborhood_explorer_base;

    [[nodiscard]]
    el::generator<Move> moves(const Solution&) const
        requires Enumerates
    {
        for (int delta = 1; delta <= 3; ++delta)
            co_yield Move{delta};
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    std::optional<Move> random_move(const Solution&, RNG& rng) const
        requires Draws
    {
        return Move{static_cast<int>(rng() % 3U) + 1};
    }

    [[nodiscard]]
    bool is_valid(const Solution&, const Move&) const
    {
        return true;
    }

    void make_move(Solution& solution, const Move& move) const
    {
        solution.score -= move.delta;
    }

    // Whether the move is forbidden by one the tabu list holds, as the lists
    // of TabuSearch ask.
    [[nodiscard]]
    bool inverse(const Solution&, const Move& move, const Move& tabu_move) const
    {
        return move.delta == -tabu_move.delta;
    }
};

// The services of a model, as an app builds them.
template<class ManagerSpec, class NeighborhoodSpec>
struct services
{
    using solution_manager_type = el::detail::service_t<ManagerSpec>;
    using neighborhood_explorer_type = el::detail::service_t<NeighborhoodSpec>;
    using context_type =
        el::detail::runner_context<solution_manager_type, neighborhood_explorer_type>;
};

template<class Manager, bool Enumerates, bool Draws, class Cost>
[[nodiscard]]
consteval auto model()
{
    const auto manager = el::solution_manager<Manager>()
        | el::cost::apply(Cost{}, el::component<ScoreComponent>());
    using manager_spec = std::remove_cvref_t<decltype(manager)>;
    using neighborhood_spec =
        decltype(el::neighborhood<Explorer<Manager, Enumerates, Draws>>());
    return services<manager_spec, neighborhood_spec>{};
}

// The same model with a cost::pareto cost, which is what Pareto Late
// Acceptance asks of its cost: with any other, its own overload reports the
// cost instead, and the capabilities are not what it stops at.
template<class Manager, bool Enumerates, bool Draws>
[[nodiscard]]
consteval auto pareto_model()
{
    const auto manager = el::solution_manager<Manager>()
        | el::cost::objectives(
            el::component<ScoreComponent>(),
            el::component<PenaltyComponent>());
    using manager_spec = std::remove_cvref_t<decltype(manager)>;
    using neighborhood_spec =
        decltype(el::neighborhood<Explorer<Manager, Enumerates, Draws>>());
    return services<manager_spec, neighborhood_spec>{};
}

struct FullCost
{
    [[nodiscard]]
    int operator()(const int value) const
    {
        return value;
    }
};

// Whether the capabilities the algorithm declares and what its run() asks of
// the services agree: it runs on them exactly when they provide every
// capability it declares.
template<class Algorithm, class Model>
consteval bool agrees()
{
    using context = typename Model::context_type;
    constexpr bool provided =
        el::runners::detail::provides_declared<Algorithm, context, std::mt19937_64&>();
    constexpr bool runs = el::detail::app_runnable_v<
        Algorithm,
        typename Model::solution_manager_type,
        typename Model::neighborhood_explorer_type>;
    return provided == runs;
}

// Every algorithm on one model.
template<class Model>
consteval bool agrees_on()
{
    static_assert(
        agrees<el::runners::BestImprovement, Model>(),
        "BestImprovement: what it declares and what its run() "
        "asks do not agree");
    static_assert(
        agrees<el::runners::FirstImprovement, Model>(),
        "FirstImprovement: what it declares and what its run() "
        "asks do not agree");
    static_assert(
        agrees<el::runners::HillClimbing, Model>(),
        "HillClimbing: what it declares and what its run() "
        "asks do not agree");
    static_assert(
        agrees<el::runners::LateAcceptanceHillClimbing, Model>(),
        "LateAcceptanceHillClimbing: what it declares and what its run() "
        "asks do not agree");
    static_assert(
        agrees<el::runners::GreatDeluge, Model>(),
        "GreatDeluge: what it declares and what its run() "
        "asks do not agree");
    static_assert(
        agrees<el::runners::SimulatedAnnealing<>, Model>(),
        "SimulatedAnnealing: what it declares and what its run() "
        "asks do not agree");
    static_assert(
        agrees<el::runners::TabuSearch<>, Model>(),
        "TabuSearch: what it declares and what its run() "
        "asks do not agree");
    static_assert(
        agrees<el::runners::FirstImprovementTabuSearch<>, Model>(),
        "FirstImprovementTabuSearch: what it declares and what its run() "
        "asks do not agree");
    static_assert(
        agrees<el::runners::AspirationPlusTabuSearch<>, Model>(),
        "AspirationPlusTabuSearch: what it declares and what its run() "
        "asks do not agree");
    static_assert(
        agrees<el::runners::EliteCandidateTabuSearch<>, Model>(),
        "EliteCandidateTabuSearch: what it declares and what its run() "
        "asks do not agree");

    return true;
}

// The models: the neighborhood enumerates, draws, or does both; the cost has
// every relation, only better, or none; the SolutionManager has a random
// solution or only the initial one.
template<class Manager, class Cost>
consteval bool agrees_on_neighborhoods()
{
    return agrees_on<decltype(model<Manager, true, true, Cost>())>()
        && agrees_on<decltype(model<Manager, true, false, Cost>())>()
        && agrees_on<decltype(model<Manager, false, true, Cost>())>();
}

template<class Manager>
consteval bool agrees_on_costs()
{
    return agrees_on_neighborhoods<Manager, FullCost>()
        && agrees_on_neighborhoods<Manager, StrictOnlyCost>()
        && agrees_on_neighborhoods<Manager, OpaqueCost>();
}

static_assert(agrees_on_costs<RandomManager>());
static_assert(agrees_on_costs<InitialOnlyManager>());

// Pareto Late Acceptance on the models of a cost::pareto cost.
template<class Manager>
consteval bool pareto_agrees_on_neighborhoods()
{
    using algorithm = el::runners::ParetoLateAcceptanceHillClimbing;
    static_assert(
        agrees<algorithm, decltype(pareto_model<Manager, true, true>())>(),
        "ParetoLateAcceptanceHillClimbing: what it declares and what its run() "
        "asks do not agree");
    static_assert(
        agrees<algorithm, decltype(pareto_model<Manager, true, false>())>(),
        "ParetoLateAcceptanceHillClimbing: what it declares and what its run() "
        "asks do not agree");
    static_assert(
        agrees<algorithm, decltype(pareto_model<Manager, false, true>())>(),
        "ParetoLateAcceptanceHillClimbing: what it declares and what its run() "
        "asks do not agree");
    return true;
}

static_assert(pareto_agrees_on_neighborhoods<RandomManager>());
static_assert(pareto_agrees_on_neighborhoods<InitialOnlyManager>());

// The models do vary the capabilities under test: a declaration that nothing
// exercises would agree with anything.
using full_model = decltype(model<RandomManager, true, true, FullCost>());
using enumerating_model = decltype(model<RandomManager, true, false, FullCost>());
using sampling_model = decltype(model<RandomManager, false, true, FullCost>());
using strict_model = decltype(model<RandomManager, true, true, StrictOnlyCost>());
using opaque_model = decltype(model<RandomManager, true, true, OpaqueCost>());
using initial_only_model = decltype(model<InitialOnlyManager, true, true, FullCost>());

template<class Need, class Model>
inline constexpr bool provides_v =
    el::runners::detail::provides<Need, typename Model::context_type, std::mt19937_64&>();

static_assert(provides_v<needs::enumerated_moves, full_model>);
static_assert(provides_v<needs::enumerated_moves, enumerating_model>);
static_assert(!provides_v<needs::enumerated_moves, sampling_model>);
static_assert(provides_v<needs::random_moves, full_model>);
static_assert(!provides_v<needs::random_moves, enumerating_model>);
static_assert(provides_v<needs::random_moves, sampling_model>);
static_assert(provides_v<needs::strict_improvement, full_model>);
static_assert(provides_v<needs::strict_improvement, strict_model>);
static_assert(!provides_v<needs::strict_improvement, opaque_model>);
static_assert(provides_v<needs::non_worsening, full_model>);
static_assert(!provides_v<needs::non_worsening, strict_model>);
static_assert(provides_v<needs::random_solutions, full_model>);
static_assert(!provides_v<needs::random_solutions, initial_only_model>);

} // namespace

int main()
{
    return 0;
}
