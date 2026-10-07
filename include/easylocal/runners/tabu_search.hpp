#pragma once

/// \file
/// Tabu Search: at each iteration the best admissible move of the neighborhood
/// is applied, even when it worsens the cost.
///
/// A tabu list forbids the moves that would undo recent ones (by the
/// neighborhood's inverse), unless an aspiration criterion lifts the
/// prohibition. The tabu lists live in runners::tabu, the aspiration criteria
/// in runners::aspiration.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost/concepts.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/trace/events.hpp>
#include <easylocal/utils/detail/attributes.hpp>
#include <easylocal/utils/detail/meta.hpp>
#include <easylocal/utils/limit.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <random>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace easylocal::runners
{

/// A candidate move, as a tabu list sees it: forbidden_by(tabu_move), whether a
/// move the list holds forbids it (the neighborhood's inverse), its attribute()
/// when the neighborhood has one, and, WithCost, its cost() and
/// equivalent_cost(other), for the lists whose state declares needs_cost =
/// true.
template<class Run, bool WithCost = false>
class tabu_candidate
{
public:
    /// The move type of the run.
    using move_type = typename Run::move_type;
    /// The solution type of the run.
    using solution_type = typename Run::solution_type;
    /// The cost type of the run.
    using cost_type = typename Run::cost_type;
    /// The neighborhood explorer type of the run.
    using neighborhood_explorer_type = typename Run::neighborhood_explorer_type;

    /// The candidate move of run at solution, not evaluated.
    ///
    /// It refers to its arguments, which must outlive it.
    tabu_candidate(
        const Run& run,
        const solution_type& solution,
        const move_type& move) noexcept
        requires(!WithCost)
        : run_{run}, solution_{solution}, move_{move}, cost_{nullptr}
    {
    }

    /// The candidate move of run at solution, evaluated: cost is the cost
    /// after it.
    ///
    /// It refers to its arguments, which must outlive it.
    tabu_candidate(
        const Run& run,
        const solution_type& solution,
        const move_type& move,
        const cost_type& cost) noexcept
        requires WithCost
        : run_{run}, solution_{solution}, move_{move}, cost_{&cost}
    {
    }

    /// The cost after the move, for the lists whose state declares needs_cost.
    [[nodiscard]]
    const cost_type& cost() const noexcept
        requires WithCost
    {
        return *cost_;
    }

    /// Whether the cost after the move is equivalent to other, by the semantics
    /// of the cost, for the lists whose state declares needs_cost.
    template<class R = Run>
        requires WithCost && requires(const R& run, const cost_type& value) {
            { run.equivalent(value, value) } -> std::convertible_to<bool>;
        }
    [[nodiscard]]
    bool equivalent_cost(const cost_type& other) const
    {
        return run_.equivalent(cost(), other);
    }

    /// The candidate move.
    [[nodiscard]]
    const move_type& move() const noexcept
    {
        return move_;
    }

    /// Whether tabu_move, a move the list holds, forbids the candidate, by the
    /// neighborhood's inverse().
    template<class R = Run>
        requires inverse_neighborhood_for<
            typename R::neighborhood_explorer_type,
            typename R::solution_type>
    [[nodiscard]]
    bool forbidden_by(const move_type& tabu_move) const
    {
        return easylocal::inverse(
            run_.neighborhood_explorer(),
            solution_,
            move_,
            tabu_move);
    }

    /// The candidate's attribute, by the neighborhood's tabu_attribute().
    [[nodiscard]]
    auto attribute() const
        requires has_tabu_attribute<neighborhood_explorer_type>
    {
        return easylocal::tabu_attribute(run_.neighborhood_explorer(), move_);
    }

private:
    const Run& run_;
    const solution_type& solution_;
    const move_type& move_;
    const cost_type* cost_;
};

/// What a tabu list learns after each iteration: the move applied, the solution
/// and its cost after it, the iteration (counted from 1), whether the move
/// improved the best cost, the move's attribute() and the solution_hash() when
/// the problem has them.
template<class Run>
class tabu_step
{
public:
    /// The move type of the run.
    using move_type = typename Run::move_type;
    /// The solution type of the run.
    using solution_type = typename Run::solution_type;
    /// The cost type of the run.
    using cost_type = typename Run::cost_type;
    /// The neighborhood explorer type of the run.
    using neighborhood_explorer_type = typename Run::neighborhood_explorer_type;

    /// The step of run that applied move, reaching solution with cost;
    /// improved_best tells whether it improved the best cost.
    ///
    /// It refers to its arguments, which must outlive it.
    tabu_step(
        const Run& run,
        const move_type& move,
        const solution_type& solution,
        const cost_type& cost,
        const bool improved_best) noexcept
        : run_{run},
          move_{move},
          solution_{solution},
          cost_{cost},
          improved_best_{improved_best}
    {
    }

    /// The move applied.
    [[nodiscard]]
    const move_type& move() const noexcept
    {
        return move_;
    }

    /// The solution after the move.
    [[nodiscard]]
    const solution_type& solution() const noexcept
    {
        return solution_;
    }

    /// The cost after the move.
    [[nodiscard]]
    const cost_type& cost() const noexcept
    {
        return cost_;
    }

    /// The iteration of the move, counted from 1.
    [[nodiscard]]
    std::size_t iteration() const noexcept
    {
        return run_.iterations();
    }

    /// Whether the move improved the best cost.
    [[nodiscard]]
    bool improved_best() const noexcept
    {
        return improved_best_;
    }

    /// The move's attribute, by the neighborhood's tabu_attribute().
    [[nodiscard]]
    auto attribute() const
        requires has_tabu_attribute<neighborhood_explorer_type>
    {
        return easylocal::tabu_attribute(run_.neighborhood_explorer(), move_);
    }

    /// The hash of the solution, by the solution manager's solution_hash().
    template<class R = Run>
        requires has_solution_hash<
            std::remove_cvref_t<decltype(std::declval<const R&>().solution_manager())>>
    [[nodiscard]]
    std::uint64_t solution_hash() const
    {
        return easylocal::solution_hash(run_.solution_manager(), solution_);
    }

    /// Whether the solution equals other, by the solution manager's solution
    /// equality.
    template<class R = Run>
        requires has_solution_equality<
            std::remove_cvref_t<decltype(std::declval<const R&>().solution_manager())>>
    [[nodiscard]]
    bool same_solution(const solution_type& other) const
    {
        return easylocal::solutions_equal(run_.solution_manager(), solution_, other);
    }

private:
    const Run& run_;
    const move_type& move_;
    const solution_type& solution_;
    const cost_type& cost_;
    bool improved_best_;
};

namespace detail
{

// Whether the state of a tabu list sees its candidates evaluated.
template<class State>
inline constexpr bool tabu_state_needs_cost_v = requires {
    requires std::remove_cvref_t<State>::needs_cost;
};

} // namespace detail

/// A tabu list policy: a value holding its parameters, from which each run
/// makes the list's state with `make_state<Run>()`.
///
/// The state answers tabu_tenure(candidate): the iterations left before the
/// candidate is no longer tabu, or nothing when it is admissible; update(step,
/// rng) records an applied move. A state may also have escape_moves(): a number
/// of random moves to apply at once, then reset to 0 (the reactive list's
/// escape).
template<class List, class Run, class RNG>
concept tabu_list_for =
    requires(const List& list) {
        typename List::parameters_type;
        { list.template make_state<Run>() };
    }
    && requires(
        decltype(std::declval<const List&>().template make_state<Run>())& state,
        const decltype(std::declval<const List&>().template make_state<Run>())&
            const_state,
        const tabu_candidate<
            Run,
            detail::tabu_state_needs_cost_v<decltype(std::declval<const List&>()
                    .template make_state<Run>())>>& candidate,
        const tabu_step<Run>& step,
        RNG& rng) {
           {
               const_state.tabu_tenure(candidate)
           } -> std::same_as<std::optional<std::size_t>>;
           { state.update(step, rng) } -> std::same_as<void>;
       };

namespace tabu
{

namespace detail
{

// A candidate whose inverse a list can test against its moves.
template<class Candidate, class Move>
concept inverse_candidate = requires(const Candidate& candidate, const Move& move) {
    { candidate.forbidden_by(move) } -> std::convertible_to<bool>;
};

// Moves with the iteration at which they leave the list (tabu while the last
// iteration is before it): the memory of the lists with per-move tenures.
template<class Move>
class expiring_moves
{
public:
    template<class Candidate>
    [[nodiscard]]
    std::optional<std::size_t> tabu_tenure(
        const Candidate& candidate,
        const std::size_t iteration) const
    {
        std::optional<std::size_t> tenure;
        for (const auto& [move, expiry] : moves_)
        {
            if (expiry > iteration && candidate.forbidden_by(move))
            {
                const auto left = expiry - iteration;
                tenure = tenure.has_value() ? (std::max)(*tenure, left) : left;
            }
        }
        return tenure;
    }

    // Records move, applied at iteration, for tenure iterations, and forgets
    // the moves whose tenure is over.
    void add(const Move& move, const std::size_t iteration, const std::size_t tenure)
    {
        std::erase_if(moves_, [iteration](const auto& entry) {
            return entry.second <= iteration;
        });
        moves_.emplace_back(move, iteration + tenure);
    }

private:
    std::vector<std::pair<Move, std::size_t>> moves_;
};

// The last moves with the iteration they were applied at, newest first: the
// memory of the lists whose length changes; a move is tabu while it is younger
// than the current length, which applies to every move held, so a list trims
// with the longest length its next update can bring.
template<class Move>
class aging_moves
{
public:
    template<class Candidate>
    [[nodiscard]]
    std::optional<std::size_t> tabu_tenure(
        const Candidate& candidate,
        const std::size_t iteration,
        const std::size_t length) const
    {
        for (const auto& [move, applied] : moves_)
        {
            const auto age = iteration - applied;
            if (age >= length)
                break;
            if (candidate.forbidden_by(move))
                return length - age;
        }
        return std::nullopt;
    }

    void add(const Move& move, const std::size_t iteration)
    {
        moves_.emplace_front(move, iteration);
    }

    // Forgets the moves no longer tabu with length.
    void trim(const std::size_t iteration, const std::size_t length)
    {
        while (!moves_.empty() && iteration - moves_.back().second >= length)
            moves_.pop_back();
    }

    // Forgets every move.
    void clear() noexcept
    {
        moves_.clear();
    }

private:
    std::deque<std::pair<Move, std::size_t>> moves_;
};

} // namespace detail

/// The parameters of the FixedLength tabu list.
struct FixedLengthParameters
{
    /// Iterations a move stays in the list.
    std::size_t tenure{10};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        using self = FixedLengthParameters;
        return config::fields(
            config::field<"tenure", &self::tenure>(
                "Number of iterations a move stays tabu",
                config::range(1, easylocal::unlimited)));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

/// TS1: a list of the last tenure moves, overwritten in a circle: each move
/// stays tabu for tenure iterations.
class FixedLength
{
public:
    /// The parameter block of the list.
    using parameters_type = FixedLengthParameters;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    explicit FixedLength(const FixedLengthParameters& parameters)
        : parameters_{config::require_valid(parameters)}
    {
    }

    /// The list of one run: tabu_tenure(candidate) tells whether a candidate is
    /// tabu, and for how many iterations; update(step, rng) records an applied
    /// move.
    template<class Move>
    class state
    {
    public:
        /// An empty list whose moves stay tabu for tenure iterations.
        explicit state(const std::size_t tenure) : tenure_{tenure}
        {
            moves_.reserve(tenure);
        }

        /// A move forbidden by several entries is tabu until the youngest
        /// expires.
        template<class Candidate>
            requires detail::inverse_candidate<Candidate, Move>
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
        {
            for (std::size_t age = 0; age < moves_.size(); ++age)
            {
                // The entry inserted age iterations before the newest.
                const auto index = (newest_ + moves_.size() - age) % moves_.size();
                if (candidate.forbidden_by(moves_[index]))
                    return tenure_ - age;
            }
            return std::nullopt;
        }

        /// Records the move, in place of the oldest one when the list holds
        /// tenure moves.
        template<class Step, class RNG>
        void update(const Step& step, RNG&)
        {
            if (moves_.size() < tenure_)
            {
                moves_.push_back(step.move());
                newest_ = moves_.size() - 1;
            }
            else
            {
                newest_ = (newest_ + 1) % tenure_;
                moves_[newest_] = step.move();
            }
        }

        /// The iterations each move stays tabu: tenure.
        [[nodiscard]]
        std::size_t current_tenure() const noexcept
        {
            return tenure_;
        }

    private:
        std::vector<Move> moves_;
        std::size_t newest_{};
        std::size_t tenure_;
    };

    /// The list's state for a run, empty.
    template<class Run>
    [[nodiscard]]
    state<typename Run::move_type> make_state() const
    {
        return state<typename Run::move_type>{parameters_.tenure};
    }

private:
    FixedLengthParameters parameters_;
};

/// The parameters of the RandomTenure tabu list.
struct RandomTenureParameters
{
    /// The shortest tenure drawn.
    std::size_t min_tenure{5};
    /// The longest tenure drawn.
    std::size_t max_tenure{15};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        using self = RandomTenureParameters;
        return config::fields(
            config::field<"min_tenure", &self::min_tenure>(
                "Minimum number of iterations a move stays tabu",
                config::range(1, easylocal::unlimited)),
            config::field<"max_tenure", &self::max_tenure>(
                "Maximum number of iterations a move stays tabu",
                config::range(1, easylocal::unlimited)),
            config::require(
                config::value<"min_tenure"> <= config::value<"max_tenure">,
                "max_tenure must not be below min_tenure"));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

/// TS2 (Gendreau, Hertz and Laporte): each move stays tabu for a tenure drawn
/// uniformly in [min_tenure, max_tenure].
class RandomTenure
{
public:
    /// The parameter block of the list.
    using parameters_type = RandomTenureParameters;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    explicit RandomTenure(const RandomTenureParameters& parameters)
        : parameters_{config::require_valid(parameters)}
    {
    }

    /// The list of one run: tabu_tenure(candidate) tells whether a candidate is
    /// tabu, and for how many iterations; update(step, rng) records an applied
    /// move.
    template<class Move>
    class state
    {
    public:
        /// An empty list, with its parameters.
        explicit state(const RandomTenureParameters& parameters) : parameters_{parameters}
        {
        }

        /// The iterations the candidate stays tabu, the most among the moves
        /// that forbid it, or nothing when none does.
        template<class Candidate>
            requires detail::inverse_candidate<Candidate, Move>
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
        {
            return moves_.tabu_tenure(candidate, iteration_);
        }

        /// Records the move, tabu for a tenure drawn uniformly in [min_tenure,
        /// max_tenure].
        template<class Step, class RNG>
        void update(const Step& step, RNG& rng)
        {
            iteration_ = step.iteration();
            std::uniform_int_distribution<std::size_t> draw{
                parameters_.min_tenure,
                parameters_.max_tenure};
            moves_.add(step.move(), iteration_, draw(rng));
        }

    private:
        RandomTenureParameters parameters_;
        detail::expiring_moves<Move> moves_;
        std::size_t iteration_{};
    };

    /// The list's state for a run, empty.
    template<class Run>
    [[nodiscard]]
    state<typename Run::move_type> make_state() const
    {
        return state<typename Run::move_type>{parameters_};
    }

private:
    RandomTenureParameters parameters_;
};

/// The parameters of the Cyclic tabu list.
struct CyclicParameters
{
    /// Iterations each tenure is used for.
    std::size_t period{100};
    /// The tenures, used in turn.
    std::vector<std::size_t> tenures{11, 34, 20, 8, 98};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        using self = CyclicParameters;
        return config::fields(
            config::field<"period", &self::period>(
                "Number of iterations each tenure is used for",
                config::range(1, easylocal::unlimited)),
            config::field<"tenures", &self::tenures>(
                "The tenures, used in turn",
                config::range(1, easylocal::unlimited)));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const
    {
        if (const auto schema = config::check_schema(*this); !schema)
            return schema;
        // The schema checks each tenure, not that there is one.
        if (tenures.empty())
            return config::validation_result::failure("tenures must not be empty");
        return config::validation_result::success();
    }
};

/// TS3 (Taillard): the tenure of the moves changes every period iterations,
/// taking the given tenures in turn.
class Cyclic
{
public:
    /// The parameter block of the list.
    using parameters_type = CyclicParameters;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    explicit Cyclic(const CyclicParameters& parameters)
        : parameters_{config::require_valid(parameters)}
    {
    }

    /// The list of one run: tabu_tenure(candidate) tells whether a candidate is
    /// tabu, and for how many iterations; update(step, rng) records an applied
    /// move.
    template<class Move>
    class state
    {
    public:
        /// An empty list, with its parameters.
        explicit state(const CyclicParameters& parameters) : parameters_{parameters} {}

        /// The iterations the candidate stays tabu, the most among the moves
        /// that forbid it, or nothing when none does.
        template<class Candidate>
            requires detail::inverse_candidate<Candidate, Move>
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
        {
            return moves_.tabu_tenure(candidate, iteration_);
        }

        /// Records the move, tabu for the current tenure, and passes to the
        /// next tenure after period moves.
        template<class Step, class RNG>
        void update(const Step& step, RNG&)
        {
            iteration_ = step.iteration();
            moves_.add(step.move(), iteration_, parameters_.tenures[current_]);
            if (++used_ == parameters_.period)
            {
                current_ = (current_ + 1) % parameters_.tenures.size();
                used_ = 0;
            }
        }

        /// The tenure the next moves get.
        [[nodiscard]]
        std::size_t current_tenure() const noexcept
        {
            return parameters_.tenures[current_];
        }

    private:
        CyclicParameters parameters_;
        detail::expiring_moves<Move> moves_;
        std::size_t iteration_{};
        std::size_t current_{};
        std::size_t used_{};
    };

    /// The list's state for a run, empty.
    template<class Run>
    [[nodiscard]]
    state<typename Run::move_type> make_state() const
    {
        return state<typename Run::move_type>{parameters_};
    }

private:
    CyclicParameters parameters_;
};

/// The parameters of the Reactive tabu list.
struct ReactiveParameters
{
    /// Factor of the tenure when a solution comes back within cycle_length
    /// iterations.
    double increase{1.1};
    /// Factor of the tenure when no cycle is seen for longer than the average
    /// cycle length.
    double decrease{0.9};
    /// Visits of a solution after which each further visit counts as chaos.
    std::size_t repetitions{3};
    /// Chaos counts after which the search escapes with random moves.
    std::size_t chaos{3};
    /// Revisits closer than this many iterations are cycles.
    std::size_t cycle_length{50};
    /// The largest tenure.
    std::size_t max_tenure{1000};
    /// Confirms a revisit by comparing the solutions with the same hash, which
    /// keeps a copy of each visited solution; it needs solution equality.
    bool verify_equality{false};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        using self = ReactiveParameters;
        return config::fields(
            config::field<"increase", &self::increase>(
                "Factor of the tenure when a solution is revisited within cycle_length",
                config::range(1.0, easylocal::unlimited).open()),
            config::field<"decrease", &self::decrease>(
                "Factor of the tenure when no cycle is seen for the average cycle length",
                config::range(0.0, 1.0).open()),
            config::field<"repetitions", &self::repetitions>(
                "Visits of a solution after which further visits count as chaos",
                config::range(0, easylocal::unlimited)),
            config::field<"chaos", &self::chaos>(
                "Chaos counts after which the search escapes with random moves",
                config::range(0, easylocal::unlimited)),
            config::field<"cycle_length", &self::cycle_length>(
                "Revisits closer than this many iterations are cycles",
                config::range(1, easylocal::unlimited)),
            config::field<"max_tenure", &self::max_tenure>(
                "The largest tenure",
                config::range(1, easylocal::unlimited)),
            config::field<"verify_equality", &self::verify_equality>(
                "Confirm revisits by comparing solutions with equal hashes"));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

/// TS4, Reactive Tabu Search (Battiti and Tecchiolli): the tenure starts at 1
/// and reacts to the solutions visited, recognized by their hash.
///
/// A solution revisited within cycle_length iterations multiplies the tenure by
/// increase and updates the average cycle length; when no such cycle occurs for
/// longer than the average, the tenure is multiplied by decrease. The tenure
/// applies to every move the list holds, so an increase makes tabu again the
/// moves that the longer tenure covers. A solution
/// visited more than repetitions times counts as chaos; after more than chaos
/// counts the memory is reset and the search escapes with 1 + (1 + r) * average
/// / 2 random moves, r uniform in [0, 1), which are recorded like the others.
/// It needs the solution hash (has_solution_hash); a collision is taken for a
/// revisit unless verify_equality keeps the solutions to compare them, which
/// needs solution equality (has_solution_equality).
class Reactive
{
public:
    /// The parameter block of the list.
    using parameters_type = ReactiveParameters;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    explicit Reactive(const ReactiveParameters& parameters)
        : parameters_{config::require_valid(parameters)}
    {
    }

    /// The list of one run: tabu_tenure(candidate) tells whether a candidate is
    /// tabu, and for how many iterations; update(step, rng) records an applied
    /// move.
    template<class Move, class Solution>
    class state
    {
    public:
        /// An empty list, with its parameters.
        explicit state(const ReactiveParameters& parameters)
            : parameters_{parameters},
              average_cycle_{static_cast<double>(parameters.cycle_length)}
        {
        }

        /// The iterations the candidate stays tabu with the current tenure, or
        /// nothing when no move still tabu forbids it.
        template<class Candidate>
            requires detail::inverse_candidate<Candidate, Move>
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
        {
            return moves_.tabu_tenure(candidate, iteration_, current_tenure());
        }

        /// Records the move and the solution reached, and adapts the tenure to
        /// the revisits.
        ///
        /// After more than chaos repeated solutions it clears its memory,
        /// resets the tenure to 1 and asks for an escape (escape_moves()).
        /// Requires the solution hash.
        template<class Step, class RNG>
            requires requires(const Step& step) {
                { step.solution_hash() } -> std::convertible_to<std::uint64_t>;
            }
        void update(const Step& step, RNG& rng)
        {
            iteration_ = step.iteration();
            moves_.add(step.move(), iteration_);
            ++since_change_;

            const std::uint64_t hash = step.solution_hash();
            auto* visit = find_visit(step, hash);
            if (visit == nullptr)
            {
                add_visit(step, hash);
            }
            else
            {
                const auto cycle = iteration_ - visit->last;
                visit->last = iteration_;
                if (++visit->count > parameters_.repetitions
                    && ++chaos_ > parameters_.chaos)
                {
                    std::uniform_real_distribution<double> draw{0.0, 1.0};
                    escape_ = 1
                        + static_cast<std::size_t>(
                            (1.0 + draw(rng)) * average_cycle_ / 2.0);
                    visits_.clear();
                    verified_visits_.clear();
                    moves_.clear();
                    chaos_ = 0;
                    tenure_ = 1.0;
                    since_change_ = 0;
                    return;
                }
                if (cycle < parameters_.cycle_length)
                {
                    tenure_ = (std::min)(tenure_ * parameters_.increase,
                        static_cast<double>(parameters_.max_tenure));
                    average_cycle_ =
                        0.9 * average_cycle_ + 0.1 * static_cast<double>(cycle);
                    since_change_ = 0;
                }
            }
            if (static_cast<double>(since_change_) > average_cycle_)
            {
                tenure_ = (std::max)(1.0, tenure_ * parameters_.decrease);
                since_change_ = 0;
            }

            // A cycle at the next update may multiply the tenure, which applies
            // to the moves held.
            const auto grown = static_cast<std::size_t>(
                std::ceil((std::min)(tenure_ * parameters_.increase,
                    static_cast<double>(parameters_.max_tenure))));
            moves_.trim(iteration_, (std::max)(current_tenure(), grown - 1));
        }

        /// The random moves of the escape asked for, 0 if none; the count is
        /// reset to 0.
        [[nodiscard]]
        std::size_t escape_moves() noexcept
        {
            return std::exchange(escape_, 0);
        }

        /// The iterations a move applied now stays tabu.
        [[nodiscard]]
        std::size_t current_tenure() const noexcept
        {
            return std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(tenure_)));
        }

    private:
        // The visits of a solution: the last one and their number.
        struct visit_record
        {
            std::size_t last;
            std::size_t count;
        };

        // With verify_equality, the solution itself, among those with its
        // hash.
        struct verified_visit
        {
            Solution solution;
            visit_record visit;
        };

        template<class Step>
        visit_record* find_visit(const Step& step, const std::uint64_t hash)
        {
            if (!parameters_.verify_equality)
            {
                const auto found = visits_.find(hash);
                return found == visits_.end() ? nullptr : &found->second;
            }
            if constexpr (requires(const Solution& solution) {
                              step.same_solution(solution);
                          })
            {
                auto [first, last] = verified_visits_.equal_range(hash);
                for (; first != last; ++first)
                    if (step.same_solution(first->second.solution))
                        return &first->second.visit;
            }
            return nullptr;
        }

        template<class Step>
        void add_visit(const Step& step, const std::uint64_t hash)
        {
            const visit_record visit{.last = iteration_, .count = 1};
            if (parameters_.verify_equality)
                verified_visits_.emplace(hash, verified_visit{step.solution(), visit});
            else
                visits_.emplace(hash, visit);
        }

        ReactiveParameters parameters_;
        // Newest first.
        detail::aging_moves<Move> moves_;
        // The visits by solution hash, without verify_equality: one node per
        // solution, which keeps no copy of it.
        std::unordered_map<std::uint64_t, visit_record> visits_;
        // The visits by solution hash with their solutions, with
        // verify_equality.
        std::unordered_multimap<std::uint64_t, verified_visit> verified_visits_;
        double tenure_{1.0};
        double average_cycle_;
        std::size_t since_change_{};
        std::size_t chaos_{};
        std::size_t escape_{};
        std::size_t iteration_{};
    };

    /// The list's state for a run, empty.
    template<class Run>
    [[nodiscard]]
    auto make_state() const
    {
        using solution_manager_type =
            std::remove_cvref_t<decltype(std::declval<const Run&>().solution_manager())>;
        if constexpr (!has_solution_equality<solution_manager_type>)
        {
            if (parameters_.verify_equality)
                throw std::invalid_argument{
                    "verify_equality needs solution equality (has_solution_equality)"};
        }
        return state<typename Run::move_type, typename Run::solution_type>{parameters_};
    }

private:
    ReactiveParameters parameters_;
};

/// The parameters of the Frequency tabu list.
struct FrequencyParameters
{
    /// Relative frequency above which an attribute is tabu.
    double threshold{0.05};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        using self = FrequencyParameters;
        return config::fields(
            config::field<"threshold", &self::threshold>(
                "Relative frequency of an attribute above which its moves are tabu",
                config::range(0.0, 1.0).open_low()));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

/// TS5, frequency-based long-term memory (Glover and Laguna's transition
/// measure): a move is tabu while the attribute of its moves (tabu_attribute)
/// has been applied in more than threshold of the iterations so far.
///
/// Its tenure is the iterations before that frequency falls to the threshold.
/// It needs the neighborhood's tabu_attribute, not its inverse.
class Frequency
{
public:
    /// The parameter block of the list.
    using parameters_type = FrequencyParameters;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    explicit Frequency(const FrequencyParameters& parameters)
        : parameters_{config::require_valid(parameters)}
    {
    }

    /// The list of one run: tabu_tenure(candidate) tells whether a candidate is
    /// tabu, and for how many iterations; update(step, rng) records an applied
    /// move.
    template<class Attribute>
    class state
    {
    public:
        /// An empty list, with its parameters.
        explicit state(const FrequencyParameters& parameters) : parameters_{parameters} {}

        /// The iterations before the frequency of the candidate's attribute
        /// falls to threshold, or nothing when it is not above it.
        template<class Candidate>
            requires requires(const Candidate& candidate) { candidate.attribute(); }
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
        {
            if (iteration_ == 0)
                return std::nullopt;
            const auto found = counts_.find(candidate.attribute());
            if (found == counts_.end())
                return std::nullopt;
            const auto count = static_cast<double>(found->second);
            if (count <= parameters_.threshold * static_cast<double>(iteration_))
                return std::nullopt;
            // The first iteration at which count / iteration <= threshold.
            const auto admissible =
                static_cast<std::size_t>(std::ceil(count / parameters_.threshold));
            return std::max<std::size_t>(1, admissible - iteration_);
        }

        /// Counts the attribute of the move applied.
        template<class Step, class RNG>
        void update(const Step& step, RNG&)
        {
            iteration_ = step.iteration();
            ++counts_[step.attribute()];
        }

    private:
        FrequencyParameters parameters_;
        std::unordered_map<Attribute, std::size_t> counts_;
        std::size_t iteration_{};
    };

    /// The list's state for a run, empty.
    template<class Run>
        requires has_tabu_attribute<typename Run::neighborhood_explorer_type>
    [[nodiscard]]
    auto make_state() const
    {
        return state<tabu_attribute_t<typename Run::neighborhood_explorer_type>>{
            parameters_};
    }

private:
    FrequencyParameters parameters_;
};

/// The parameters of the ObjectiveBased tabu list.
struct ObjectiveBasedParameters
{
    /// Iterations a cost value stays tabu.
    std::size_t tenure{10};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        using self = ObjectiveBasedParameters;
        return config::fields(
            config::field<"tenure", &self::tenure>(
                "Number of iterations a reached cost stays tabu",
                config::range(1, easylocal::unlimited)));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

/// Tabu on cost values (Gendreau and Potvin): a move is tabu when it would
/// reach a cost equivalent to one reached in the last tenure iterations, by
/// the semantics of the cost (equivalent()).
///
/// It needs neither inverse nor attribute, but the candidate's cost: moves are
/// evaluated before the tabu check.
class ObjectiveBased
{
public:
    /// The parameter block of the list.
    using parameters_type = ObjectiveBasedParameters;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    explicit ObjectiveBased(const ObjectiveBasedParameters& parameters)
        : parameters_{config::require_valid(parameters)}
    {
    }

    /// The list of one run: tabu_tenure(candidate) tells whether a candidate is
    /// tabu, and for how many iterations; update(step, rng) records an applied
    /// move.
    template<class Cost>
    class state
    {
    public:
        /// The candidates carry their cost, which the list compares.
        static constexpr bool needs_cost = true;

        /// An empty list whose moves stay tabu for tenure iterations.
        explicit state(const std::size_t tenure) : tenure_{tenure} {}

        /// The iterations the candidate's cost stays tabu when it is
        /// equivalent to one of the last tenure costs reached, or nothing.
        template<class Candidate>
            requires requires(const Candidate& candidate, const Cost& cost) {
                { candidate.equivalent_cost(cost) } -> std::convertible_to<bool>;
            }
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
        {
            for (std::size_t age = 0; age < costs_.size(); ++age)
                if (candidate.equivalent_cost(costs_[age]))
                    return tenure_ - age;
            return std::nullopt;
        }

        /// Records the cost reached, keeping the last tenure ones.
        template<class Step, class RNG>
        void update(const Step& step, RNG&)
        {
            costs_.push_front(step.cost());
            if (costs_.size() > tenure_)
                costs_.pop_back();
        }

    private:
        // Newest first.
        std::deque<Cost> costs_;
        std::size_t tenure_;
    };

    /// The list's state for a run, empty.
    template<class Run>
    [[nodiscard]]
    state<typename Run::cost_type> make_state() const
    {
        return state<typename Run::cost_type>{parameters_.tenure};
    }

private:
    ObjectiveBasedParameters parameters_;
};

/// The parameters of the LimDynamic (limited dynamic tenure) tabu list.
struct LimDynamicParameters
{
    /// The tenure after an improvement of the best cost.
    std::size_t min_tenure{5};
    /// The tenure at which it falls back to min_tenure.
    std::size_t max_tenure{20};
    /// Iterations without improving the best cost after which the tenure grows.
    std::size_t idle_threshold{10};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        using self = LimDynamicParameters;
        return config::fields(
            config::field<"min_tenure", &self::min_tenure>(
                "The tenure after an improvement of the best cost",
                config::range(1, easylocal::unlimited)),
            config::field<"max_tenure", &self::max_tenure>(
                "The tenure at which it falls back to min_tenure",
                config::range(1, easylocal::unlimited)),
            config::field<"idle_threshold", &self::idle_threshold>(
                "Iterations without improvement after which the tenure grows",
                config::range(0, easylocal::unlimited)),
            config::require(
                config::value<"min_tenure"> < config::value<"max_tenure">,
                "max_tenure must exceed min_tenure"));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

/// LimDynamic, a limited dynamic tenure: it grows by one at each iteration
/// after idle_threshold iterations without improving the best cost, and falls
/// back to min_tenure when the best improves or the tenure reaches max_tenure.
///
/// The name is the one of Da Ros, Di Gaspero and Schaerf's performance analysis
/// of tabu list strategies, where it is compared with the other lists of
/// easylocal::runners::tabu.
class LimDynamic
{
public:
    /// The parameter block of the list.
    using parameters_type = LimDynamicParameters;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    explicit LimDynamic(const LimDynamicParameters& parameters)
        : parameters_{config::require_valid(parameters)}
    {
    }

    /// The list of one run: tabu_tenure(candidate) tells whether a candidate is
    /// tabu, and for how many iterations; update(step, rng) records an applied
    /// move.
    template<class Move>
    class state
    {
    public:
        /// An empty list, with its parameters.
        explicit state(const LimDynamicParameters& parameters)
            : parameters_{parameters}, tenure_{parameters.min_tenure}
        {
        }

        /// The iterations the candidate stays tabu with the current tenure, or
        /// nothing when no move still tabu forbids it.
        template<class Candidate>
            requires detail::inverse_candidate<Candidate, Move>
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
        {
            return moves_.tabu_tenure(candidate, iteration_, tenure_);
        }

        /// Records the move, and resets the tenure to min_tenure on an
        /// improvement of the best cost or at max_tenure, or grows it by one
        /// after idle_threshold idle iterations.
        template<class Step, class RNG>
        void update(const Step& step, RNG&)
        {
            iteration_ = step.iteration();
            moves_.add(step.move(), iteration_);
            idle_ = step.improved_best() ? 0 : idle_ + 1;
            if (step.improved_best() || tenure_ >= parameters_.max_tenure)
                tenure_ = parameters_.min_tenure;
            else if (idle_ >= parameters_.idle_threshold)
                ++tenure_;
            moves_.trim(iteration_, tenure_);
        }

        /// The iterations a move applied now stays tabu.
        [[nodiscard]]
        std::size_t current_tenure() const noexcept
        {
            return tenure_;
        }

    private:
        LimDynamicParameters parameters_;
        detail::aging_moves<Move> moves_;
        std::size_t tenure_;
        std::size_t idle_{};
        std::size_t iteration_{};
    };

    /// The list's state for a run, empty.
    template<class Run>
    [[nodiscard]]
    state<typename Run::move_type> make_state() const
    {
        return state<typename Run::move_type>{parameters_};
    }

private:
    LimDynamicParameters parameters_;
};

namespace detail
{

// The Fluctuation Of the Objective of Blöchliger and Zufferey: at the end of
// each window of iterations, the tenure grows by increment if the costs reached
// in the window spread less than fluctuation, and shrinks by one (to 1 at
// least) otherwise. The spread is cost::delta(highest, lowest). After each
// update the list calls trim() with the window and the increment to come.
template<class Move, class Cost>
class fluctuation_tenure
{
public:
    template<class Candidate>
    [[nodiscard]]
    std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
    {
        return moves_.tabu_tenure(candidate, iteration_, tenure_);
    }

    // Records a move; true at the end of a window, after the tenure changed.
    template<class Step>
    bool update(
        const Step& step,
        const std::size_t window,
        const std::size_t increment,
        const double fluctuation)
    {
        using cost::delta;
        iteration_ = step.iteration();
        moves_.add(step.move(), iteration_);
        const auto& cost = step.cost();
        if (!lowest_.has_value())
        {
            lowest_ = cost;
            highest_ = cost;
        }
        else
        {
            if (static_cast<double>(delta(cost, *lowest_)) < 0.0)
                lowest_ = cost;
            if (static_cast<double>(delta(cost, *highest_)) > 0.0)
                highest_ = cost;
        }

        bool window_over = false;
        if (++in_window_ >= window)
        {
            const auto spread = static_cast<double>(delta(*highest_, *lowest_));
            tenure_ = spread < fluctuation
                ? tenure_ + increment
                : std::max<std::size_t>(1, tenure_ - 1);
            lowest_.reset();
            highest_.reset();
            in_window_ = 0;
            window_over = true;
        }
        return window_over;
    }

    // Forgets the moves the next update cannot make tabu: those younger than
    // tenure + increment - 1 stay when it ends a window of this length, since
    // the growth applies to them too.
    void trim(const std::size_t window, const std::size_t increment)
    {
        const auto window_ends = in_window_ + 1 >= window;
        moves_.trim(iteration_, window_ends ? tenure_ + increment - 1 : tenure_);
    }

    void set_tenure(const std::size_t tenure) noexcept
    {
        tenure_ = tenure;
    }

    [[nodiscard]]
    std::size_t current_tenure() const noexcept
    {
        return tenure_;
    }

private:
    aging_moves<Move> moves_;
    std::optional<Cost> lowest_;
    std::optional<Cost> highest_;
    // 0 until set: no move is tabu.
    std::size_t tenure_{};
    std::size_t in_window_{};
    std::size_t iteration_{};
};

} // namespace detail

/// The parameters of the Foo (Fluctuation Of the Objective) tabu list.
struct FooParameters
{
    /// Iterations between two tenure changes.
    std::size_t window{100};
    /// Growth of the tenure when the costs fluctuate little; also the initial
    /// tenure.
    std::size_t increment{5};
    /// The spread of the costs in a window below which the tenure grows.
    double fluctuation{1.0};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        using self = FooParameters;
        return config::fields(
            config::field<"window", &self::window>(
                "Iterations between two tenure changes",
                config::range(1, easylocal::unlimited)),
            config::field<"increment", &self::increment>(
                "Growth of the tenure when the costs fluctuate little (the initial tenure)",
                config::range(1, easylocal::unlimited)),
            config::field<"fluctuation", &self::fluctuation>(
                "Spread of the costs in a window below which the tenure grows",
                config::range(0.0, easylocal::unlimited).open_high()));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

/// FOO, the Fluctuation Of the Objective scheme of Blöchliger and Zufferey's
/// reactive tabu search for graph coloring: a tenure that grows by increment
/// when the costs reached in the last window spread less than fluctuation (the
/// search is stuck), and shrinks by one otherwise.
///
/// The fluctuation is in cost units, so it depends on the instance; the cost
/// needs cost::delta. With a cost::hierarchical cost, the spread of a window
/// whose hard cost changed is infinite, the delta between two hard levels, and
/// the tenure shrinks. The tenure applies to every move the list holds, so a
/// growth makes tabu again the moves of the last tenure + increment - 1
/// iterations.
class Foo
{
public:
    /// The parameter block of the list.
    using parameters_type = FooParameters;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    explicit Foo(const FooParameters& parameters)
        : parameters_{config::require_valid(parameters)}
    {
    }

    /// The list of one run: tabu_tenure(candidate) tells whether a candidate is
    /// tabu, and for how many iterations; update(step, rng) records an applied
    /// move.
    template<class Move, class Cost>
    class state
    {
    public:
        /// An empty list, with its parameters.
        explicit state(const FooParameters& parameters) : parameters_{parameters}
        {
            tenure_.set_tenure(parameters.increment);
        }

        /// The iterations the candidate stays tabu with the current tenure, or
        /// nothing when no move still tabu forbids it.
        template<class Candidate>
            requires detail::inverse_candidate<Candidate, Move>
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
        {
            return tenure_.tabu_tenure(candidate);
        }

        /// Records the move and its cost and, at the end of a window, grows the
        /// tenure by increment if the costs spread less than fluctuation, or
        /// shrinks it by one.
        template<class Step, class RNG>
        void update(const Step& step, RNG&)
        {
            tenure_.update(
                step,
                parameters_.window,
                parameters_.increment,
                parameters_.fluctuation);
            tenure_.trim(parameters_.window, parameters_.increment);
        }

        /// The iterations a move applied now stays tabu.
        [[nodiscard]]
        std::size_t current_tenure() const noexcept
        {
            return tenure_.current_tenure();
        }

    private:
        FooParameters parameters_;
        detail::fluctuation_tenure<Move, Cost> tenure_;
    };

    /// The list's state for a run, empty.
    template<class Run>
        requires cost::has_delta<typename Run::cost_type>
    [[nodiscard]]
    state<typename Run::move_type, typename Run::cost_type> make_state() const
    {
        return state<typename Run::move_type, typename Run::cost_type>{parameters_};
    }

private:
    FooParameters parameters_;
};

/// The parameters of the RandomFoo (randomized Fluctuation Of the Objective)
/// tabu list.
struct RandomFooParameters
{
    /// The smallest window drawn.
    std::size_t min_window{50};
    /// The largest window drawn.
    std::size_t max_window{150};
    /// The smallest increment drawn.
    std::size_t min_increment{2};
    /// The largest increment drawn.
    std::size_t max_increment{8};
    /// The smallest fluctuation threshold drawn.
    double min_fluctuation{0.5};
    /// The largest fluctuation threshold drawn.
    double max_fluctuation{2.0};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        using self = RandomFooParameters;
        return config::fields(
            config::field<"min_window", &self::min_window>(
                "Smallest window",
                config::range(1, easylocal::unlimited)),
            config::field<"max_window", &self::max_window>(
                "Largest window",
                config::range(1, easylocal::unlimited)),
            config::field<"min_increment", &self::min_increment>(
                "Smallest increment",
                config::range(1, easylocal::unlimited)),
            config::field<"max_increment", &self::max_increment>(
                "Largest increment",
                config::range(1, easylocal::unlimited)),
            config::field<"min_fluctuation", &self::min_fluctuation>(
                "Smallest fluctuation threshold",
                config::range(0.0, easylocal::unlimited).open_high()),
            config::field<"max_fluctuation", &self::max_fluctuation>(
                "Largest fluctuation threshold",
                config::range(0.0, easylocal::unlimited).open_high()),
            config::require(
                config::value<"min_window"> <= config::value<"max_window">,
                "min_window must not be above max_window"),
            config::require(
                config::value<"min_increment"> <= config::value<"max_increment">,
                "min_increment must not be above max_increment"),
            config::require(
                config::value<"min_fluctuation"> <= config::value<"max_fluctuation">,
                "min_fluctuation must not be above max_fluctuation"));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

/// RandomFoo, a randomized Fluctuation Of the Objective: Foo with its window,
/// increment and fluctuation drawn uniformly in their ranges at the start and
/// again at the end of each window.
class RandomFoo
{
public:
    /// The parameter block of the list.
    using parameters_type = RandomFooParameters;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when they are not valid.
    explicit RandomFoo(const RandomFooParameters& parameters)
        : parameters_{config::require_valid(parameters)}
    {
    }

    /// The list of one run: tabu_tenure(candidate) tells whether a candidate is
    /// tabu, and for how many iterations; update(step, rng) records an applied
    /// move.
    template<class Move, class Cost>
    class state
    {
    public:
        /// An empty list, with its parameters.
        explicit state(const RandomFooParameters& parameters) : parameters_{parameters} {}

        /// The iterations the candidate stays tabu with the current tenure, or
        /// nothing when no move still tabu forbids it.
        template<class Candidate>
            requires detail::inverse_candidate<Candidate, Move>
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
        {
            return tenure_.tabu_tenure(candidate);
        }

        /// Records the move as Foo does, with window, increment and fluctuation
        /// drawn at the first move and again at the end of each window.
        template<class Step, class RNG>
        void update(const Step& step, RNG& rng)
        {
            if (!drawn_)
            {
                draw(rng);
                tenure_.set_tenure(increment_);
                drawn_ = true;
            }
            if (tenure_.update(step, window_, increment_, fluctuation_))
                draw(rng);
            tenure_.trim(window_, increment_);
        }

        /// The iterations a move applied now stays tabu; 0 before the first
        /// move, which draws it.
        [[nodiscard]]
        std::size_t current_tenure() const noexcept
        {
            return tenure_.current_tenure();
        }

    private:
        template<class RNG>
        void draw(RNG& rng)
        {
            window_ = std::uniform_int_distribution<std::size_t>{
                parameters_.min_window,
                parameters_.max_window}(rng);
            increment_ = std::uniform_int_distribution<std::size_t>{
                parameters_.min_increment,
                parameters_.max_increment}(rng);
            fluctuation_ = parameters_.min_fluctuation == parameters_.max_fluctuation
                ? parameters_.min_fluctuation
                : std::uniform_real_distribution<double>{
                      parameters_.min_fluctuation,
                      parameters_.max_fluctuation}(rng);
        }

        RandomFooParameters parameters_;
        detail::fluctuation_tenure<Move, Cost> tenure_;
        std::size_t window_{};
        std::size_t increment_{};
        double fluctuation_{};
        bool drawn_{false};
    };

    /// The list's state for a run, empty.
    template<class Run>
        requires cost::has_delta<typename Run::cost_type>
    [[nodiscard]]
    state<typename Run::move_type, typename Run::cost_type> make_state() const
    {
        return state<typename Run::move_type, typename Run::cost_type>{parameters_};
    }

private:
    RandomFooParameters parameters_;
};

} // namespace tabu

/// An aspiration criterion of Tabu Search: overrides(run, candidate, best)
/// tells whether a tabu move of cost candidate is admitted, when the best cost
/// found is best; needs_cost tells whether the search must evaluate the tabu
/// moves for it (false: never admitted, as aspiration::None).
template<class Aspiration, class Run>
concept aspiration_for = requires(
    const Aspiration& aspiration,
    const Run& run,
    const typename Run::cost_type& cost) {
    { Aspiration::needs_cost } -> std::convertible_to<bool>;
    { aspiration.overrides(run, cost, cost) } -> std::convertible_to<bool>;
};

namespace aspiration
{

/// A tabu move is admitted when it would improve the best cost found.
struct ByObjective
{
    /// Tabu candidates are evaluated, for overrides().
    static constexpr bool needs_cost = true;

    /// Whether the candidate cost is better than the best cost.
    template<class Run, class Cost>
    [[nodiscard]]
    bool overrides(const Run& run, const Cost& candidate, const Cost& best) const
    {
        return run.better(candidate, best);
    }
};

/// Tabu moves are never admitted, and need not be evaluated.
struct None
{
    /// Tabu candidates need not be evaluated.
    static constexpr bool needs_cost = false;

    /// Never: a tabu move stays forbidden.
    template<class Run, class Cost>
    [[nodiscard]]
    bool overrides(const Run&, const Cost&, const Cost&) const
    {
        return false;
    }
};

} // namespace aspiration

namespace candidates
{

/// The parameters of the full scan of TabuSearch: none.
struct FullParameters
{
    /// The names, members and descriptions of the parameters: none.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields();
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        return config::validation_result::success();
    }
};

/// The parameters of the scan of FirstImprovementTabuSearch.
struct FirstImprovementParameters
{
    /// The scan stops at the first admissible move that improves the best cost
    /// rather than the current one.
    bool improve_on_best{false};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        using self = FirstImprovementParameters;
        return config::fields(
            config::field<"improve_on_best", &self::improve_on_best>(
                "Stop the scan at a move improving the best cost, not the current one"));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

/// The parameters of Glover's aspiration plus candidate strategy, the scan of
/// AspirationPlusTabuSearch.
struct AspirationPlusParameters
{
    /// Admissible moves examined at least in each scan.
    std::size_t min_moves{10};
    /// Admissible moves examined at most in each scan.
    std::size_t max_moves{100};
    /// Admissible moves examined after the first one under the aspiration
    /// level.
    std::size_t plus{5};
    /// The aspiration level, as a factor of the best cost (see
    /// AspirationPlusTabuSearch).
    double aspiration_level{1.0};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        using self = AspirationPlusParameters;
        return config::fields(
            config::field<"min_moves", &self::min_moves>(
                "Admissible moves examined at least in each scan",
                config::range(1, easylocal::unlimited)),
            config::field<"max_moves", &self::max_moves>(
                "Admissible moves examined at most in each scan",
                config::range(1, easylocal::unlimited)),
            config::field<"plus", &self::plus>(
                "Admissible moves examined after the first under the aspiration level",
                config::range(0, easylocal::unlimited)),
            config::field<"aspiration_level", &self::aspiration_level>(
                "The aspiration level, as a factor of the best cost",
                config::range(1.0, easylocal::unlimited).open_high()),
            config::require(
                config::value<"min_moves"> <= config::value<"max_moves">,
                "min_moves must not be above max_moves"));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

/// The parameters of Glover's elite candidate list, the scan of
/// EliteCandidateTabuSearch.
struct EliteListParameters
{
    /// The moves kept from a full scan.
    std::size_t elite_size{10};
    /// A kept move is applied while its cost is within this factor of the best
    /// cost (see EliteCandidateTabuSearch).
    double quality{1.05};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        using self = EliteListParameters;
        return config::fields(
            config::field<"elite_size", &self::elite_size>(
                "The moves kept from a full scan",
                config::range(1, easylocal::unlimited)),
            config::field<"quality", &self::quality>(
                "Cost, as a factor of the best, up to which a kept move is applied",
                config::range(1.0, easylocal::unlimited).open_high()));
    }

    /// Whether the parameters are valid, and why not.
    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

} // namespace candidates

/// The parameters of the tabu searches: the limits of the run, those of the
/// tabu list as the group tabu_list and those of the candidate strategy as the
/// group candidates.
///
/// The full scan of TabuSearch, candidates::FullParameters, has no parameters
/// and no group.
template<class ListParameters, class CandidateParameters = candidates::FullParameters>
struct TabuSearchParameters
{
    /// Iterations without improving the best cost after which the search
    /// stops; unlimited: it never stops for that.
    limit max_idle_iterations{1000};
    /// Iterations in all; unlimited by default.
    limit max_iterations{unlimited};
    /// Evaluation budget, including the initial evaluation; unlimited by
    /// default.
    limit max_evaluations{unlimited};
    /// The parameters of the tabu list.
    ListParameters tabu_list{};
    /// The parameters of the candidate strategy, the scan of each iteration.
    CandidateParameters candidates{};

    /// The names, members and descriptions of the parameters.
    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        using self = TabuSearchParameters;
        const auto common = config::fields(
            config::field<"max_idle_iterations", &self::max_idle_iterations>(
                "Maximum number of iterations without improving the best cost, or "
                "unlimited",
                config::range(1, easylocal::unlimited)),
            config::field<"max_iterations", &self::max_iterations>(
                "Maximum number of iterations, or unlimited",
                config::range(0, easylocal::unlimited)),
            config::field<"max_evaluations", &self::max_evaluations>(
                "Maximum number of solution evaluations, or unlimited",
                config::range(0, easylocal::unlimited)),
            config::group<"tabu_list", &self::tabu_list>("The tabu list"));
        if constexpr (std::tuple_size_v<decltype(CandidateParameters::parameter_schema())>
            == 0)
        {
            return common;
        }
        else
        {
            const auto group =
                config::group<"candidates", &self::candidates>("The candidate strategy");
            return std::tuple_cat(common, std::tuple<decltype(group)>{group});
        }
    }

    /// Whether the parameters are valid, and why not: the tabu list's and the
    /// candidate strategy's too, as groups.
    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        return config::check_schema(*this);
    }
};

namespace detail
{

template<class Context>
concept tabu_search_context = enumerating_strict_improvement_context<Context>;

// A level of the cost within factor (at least 1) of best: factor times best
// when best is positive, as far above it when it is zero or negative, so that
// the level is never better than best.
[[nodiscard]]
inline double relaxed_level(const double best, const double factor) noexcept
{
    return best + (factor - 1.0) * std::abs(best);
}

// A list that escapes with random moves needs a neighborhood that draws them.
template<class List, class Run, class RNG>
concept tabu_escape_supported =
    !requires(decltype(std::declval<const List&>().template make_state<Run>())& state) {
        state.escape_moves();
    } || random_move_context<typename Run::context_type, RNG>;

// The state of one tabu search: the current solution and its evaluation, the
// best solution and its cost, the list's state and the idle iterations.
template<class Run, class ListState>
struct tabu_run
{
    typename Run::solution_type solution;
    typename Run::evaluation_type current;
    best_so_far<typename Run::solution_type, typename Run::cost_type> best;
    ListState list;
    std::size_t idle_iterations{};
};

// The outcome of a scan: the chosen admissible candidate and its move, with
// its position among the scanned moves; the least tabu move; whether there
// was no move at all, and whether the run had to stop during the scan.
template<class Run>
struct tabu_scan
{
    std::optional<typename Run::candidate_type> chosen;
    std::optional<typename Run::move_type> chosen_move;
    std::size_t chosen_position{};
    // The chosen move was tabu, admitted by the aspiration criterion.
    bool chosen_aspirated{false};
    std::optional<typename Run::move_type> least_tabu;
    // The evaluation of the least tabu move, when the scan made one.
    std::optional<typename Run::candidate_type> least_tabu_candidate;
    bool empty{true};
    bool interrupted{false};
};

// The machinery common to the tabu searches: start, the limits, a scan of any
// range of moves, the application of the chosen move, and the whole search
// for the runners that scan the neighborhood.
template<class TabuList, class Aspiration>
class tabu_search_engine
{
public:
    template<class Parameters>
    tabu_search_engine(const Parameters& parameters, Aspiration aspiration)
        : max_idle_iterations_{parameters.max_idle_iterations},
          max_iterations_{parameters.max_iterations},
          max_evaluations_{parameters.max_evaluations},
          tabu_list_{parameters.tabu_list},
          aspiration_{std::move(aspiration)}
    {
    }

    template<class Run>
    [[nodiscard]]
    auto start(Run& run, typename Run::solution_type solution) const
    {
        run.limit_evaluations(max_evaluations_);
        // Before the run starts: making the list may throw (Reactive's
        // verify_equality without solution equality).
        auto list = tabu_list_.template make_state<Run>();
        auto current = run.start(solution);
        auto best = best_so_far{solution, current.cost()};
        tabu_run<Run, decltype(list)> state{
            .solution = std::move(solution),
            .current = std::move(current),
            .best = std::move(best),
            .list = std::move(list),
        };
        // The tenure at the start, unless the list sets it at the first move
        // (RandomFoo draws it).
        if constexpr (requires { state.list.current_tenure(); })
        {
            if (const auto tenure = state.list.current_tenure(); tenure != 0)
                tenure_changed(run, 0, tenure);
        }
        return state;
    }

    // The idle and iteration limits, checked before each iteration.
    template<class Run, class State>
    [[nodiscard]]
    std::optional<termination_reason> limit_reached(const Run& run, const State& state)
        const
    {
        if (state.idle_iterations >= max_idle_iterations_)
            return termination_reason::idle_limit_reached;
        if (run.iterations() >= max_iterations_)
            return termination_reason::completed;
        return std::nullopt;
    }

    // Scans moves: the best admissible candidate, ties broken uniformly at
    // random (each of k equivalent candidates replaces the choice with
    // probability 1/k), until stop_at(run, candidate, current, best) holds;
    // on_admissible(move, cost, position) sees every admissible candidate. A
    // tie is an equivalent cost: with a partial order (a cost::pareto cost) an
    // unordered candidate does not replace the choice.
    template<
        class Run,
        class State,
        class Moves,
        class StopAt,
        class RNG,
        class OnAdmissible>
    [[nodiscard]]
    tabu_scan<Run> scan(
        Run& run,
        State& state,
        Moves&& moves,
        StopAt&& stop_at,
        RNG& rng,
        OnAdmissible&& on_admissible) const
    {
        constexpr bool list_needs_cost =
            detail::tabu_state_needs_cost_v<decltype(state.list)>;

        tabu_scan<Run> result;
        std::size_t ties = 0;
        std::size_t least_tenure = 0;
        std::size_t least_ties = 0;
        std::size_t position = 0;

        for (const auto& move : moves)
        {
            if (run.should_stop())
            {
                result.interrupted = true;
                return result;
            }
            result.empty = false;
            const auto here = position++;

            // A list on costs sees the candidate evaluated.
            std::optional<typename Run::candidate_type> evaluated;
            const auto tenure = [&] {
                if constexpr (list_needs_cost)
                {
                    evaluated = run.evaluate_move(state.solution, state.current, move);
                    return state.list.tabu_tenure(
                        tabu_candidate<Run, true>{
                            run,
                            state.solution,
                            move,
                            evaluated->cost()});
                }
                else
                {
                    return state.list.tabu_tenure(
                        tabu_candidate<Run>{run, state.solution, move});
                }
            }();
            // Whether this move is now the least tabu one, whose evaluation, if
            // it has one, apply() then reuses.
            bool least_here = false;
            if (tenure.has_value())
            {
                if (!result.least_tabu.has_value() || *tenure < least_tenure)
                {
                    least_here = true;
                    least_tenure = *tenure;
                    least_ties = 1;
                }
                else if (*tenure == least_tenure && draw(rng, ++least_ties) == 0)
                {
                    least_here = true;
                }
                if (least_here)
                {
                    result.least_tabu = move;
                    result.least_tabu_candidate.reset();
                }
                if constexpr (!Aspiration::needs_cost)
                {
                    if (least_here && evaluated.has_value())
                        result.least_tabu_candidate = std::move(evaluated);
                    continue;
                }
            }

            auto candidate = evaluated.has_value()
                ? std::move(*evaluated)
                : run.evaluate_move(state.solution, state.current, move);
            if (tenure.has_value()
                && !aspiration_.overrides(run, candidate.cost(), state.best.cost))
            {
                if (least_here)
                    result.least_tabu_candidate = std::move(candidate);
                continue;
            }

            on_admissible(move, candidate.cost(), here);
            const auto stop =
                stop_at(run, candidate.cost(), state.current.cost(), state.best.cost);
            if (!result.chosen.has_value()
                || run.better(candidate.cost(), result.chosen->cost()))
            {
                result.chosen = std::move(candidate);
                result.chosen_move = move;
                result.chosen_position = here;
                result.chosen_aspirated = tenure.has_value();
                ties = 1;
            }
            else if (tie(run, candidate.cost(), result.chosen->cost())
                && draw(rng, ++ties) == 0)
            {
                result.chosen = std::move(candidate);
                result.chosen_move = move;
                result.chosen_position = here;
                result.chosen_aspirated = tenure.has_value();
            }
            if (stop)
                break;
        }
        return result;
    }

    // Applies the scan's choice or, when every move was tabu, the least tabu
    // move; updates the best solution, the idle count and the list, and makes
    // the list's escape. False when the run had to stop first.
    template<class Run, class State, class RNG>
    bool apply(Run& run, State& state, tabu_scan<Run>&& scan, RNG& rng) const
    {
        if (!scan.chosen.has_value())
        {
            // Every move was tabu: the least tabu one, evaluated by the scan or
            // now.
            assert(scan.least_tabu.has_value());
            if (scan.least_tabu_candidate.has_value())
                scan.chosen = std::move(scan.least_tabu_candidate);
            else
            {
                if (run.should_stop())
                    return false;
                scan.chosen =
                    run.evaluate_move(state.solution, state.current, *scan.least_tabu);
            }
            scan.chosen_move = std::move(scan.least_tabu);
        }

        commit(run, state, std::move(*scan.chosen), *scan.chosen_move);
        if (scan.chosen_aspirated)
        {
            run.emit(
                trace::event::aspiration_applied<typename Run::cost_type>{
                    .evaluations = run.evaluations(),
                    .iterations = run.iterations(),
                    .cost = state.current.cost(),
                });
        }
        update_list(run, state, *scan.chosen_move, rng);

        // The reactive list's escape: random moves, applied whatever their
        // cost and recorded in the list like the others, then traced with the
        // number applied (the run may stop first).
        if constexpr (requires { state.list.escape_moves(); })
        {
            const auto escape_moves = state.list.escape_moves();
            std::size_t applied = 0;
            for (; applied < escape_moves; ++applied)
            {
                if (run.should_stop() || run.iterations() >= max_iterations_)
                {
                    break;
                }
                auto move = run.random_move(state.solution, rng);
                if (!move.has_value())
                    break;
                auto candidate = run.evaluate_move(state.solution, state.current, *move);
                commit(run, state, std::move(candidate), *move);
                update_list(run, state, *move, rng);
            }
            if (applied > 0)
            {
                run.emit(
                    trace::event::tabu_escape{
                        .evaluations = run.evaluations(),
                        .iterations = run.iterations(),
                        .moves = applied,
                    });
            }
        }
        return true;
    }

    template<class Run, class State>
    [[nodiscard]]
    auto finish(Run& run, State& state) const
    {
        return run.finish(std::move(state.best.solution), std::move(state.best.cost));
    }

    template<class Run, class State>
    [[nodiscard]]
    auto finish(Run& run, State& state, const termination_reason reason) const
    {
        return run
            .finish(std::move(state.best.solution), std::move(state.best.cost), reason);
    }

    // The search of the runners that scan the whole neighborhood: make_stop(run,
    // best.cost) gives, for each scan, the callable that decides at each
    // admissible candidate whether the scan stops there.
    template<class Run, class RNG, class MakeStop>
    [[nodiscard]]
    auto search(
        Run& run,
        typename Run::solution_type solution,
        RNG& rng,
        MakeStop make_stop) const
    {
        auto state = start(run, std::move(solution));
        while (!run.should_stop())
        {
            if (const auto reason = limit_reached(run, state))
                return finish(run, state, *reason);

            auto result = scan(
                run,
                state,
                run.moves(state.solution),
                make_stop(run, state.best.cost),
                rng,
                [](const auto&...) {});
            if (result.interrupted)
                break;
            if (result.empty)
                return finish(run, state, termination_reason::local_optimum);
            if (!apply(run, state, std::move(result), rng))
                break;
        }
        return finish(run, state);
    }

private:
    // Applies a move as an iteration and updates the best solution and the
    // idle count.
    template<class Run, class State>
    static void commit(
        Run& run,
        State& state,
        typename Run::candidate_type&& candidate,
        const typename Run::move_type& move)
    {
        run.next_iteration();
        run.commit(state.solution, state.current, std::move(candidate), move);
        if (state.best.update(run, state.solution, state.current))
        {
            state.idle_iterations = 0;
        }
        else
        {
            ++state.idle_iterations;
        }
    }

    // Records the move just applied in the list, and traces a change of its
    // tenure when the list has one tenure for all moves.
    template<class Run, class State, class RNG>
    static void update_list(
        Run& run,
        State& state,
        const typename Run::move_type& move,
        RNG& rng)
    {
        const tabu_step<Run> step{
            run,
            move,
            state.solution,
            state.current.cost(),
            state.idle_iterations == 0};
        if constexpr (requires { state.list.current_tenure(); })
        {
            const auto previous = state.list.current_tenure();
            state.list.update(step, rng);
            if (const auto tenure = state.list.current_tenure(); tenure != previous)
                tenure_changed(run, previous, tenure);
        }
        else
        {
            state.list.update(step, rng);
        }
    }

    template<class Run>
    static void tenure_changed(
        Run& run,
        const std::size_t previous_tenure,
        const std::size_t tenure)
    {
        run.emit(
            trace::event::tabu_tenure_changed{
                .evaluations = run.evaluations(),
                .iterations = run.iterations(),
                .previous_tenure = previous_tenure,
                .tenure = tenure,
            });
    }

    // Whether candidate, not better than chosen, ties with it: equivalent by
    // the cost semantics or, without equivalent(), not worse.
    template<class Run, class Cost>
    [[nodiscard]]
    static bool tie(const Run& run, const Cost& candidate, const Cost& chosen)
    {
        if constexpr (requires {
                          {
                              run.equivalent(candidate, chosen)
                          } -> std::convertible_to<bool>;
                      })
            return run.equivalent(candidate, chosen);
        else
            return !run.better(chosen, candidate);
    }

    // Uniform in [0, count).
    template<class RNG>
    [[nodiscard]]
    static std::size_t draw(RNG& rng, const std::size_t count)
    {
        return std::uniform_int_distribution<std::size_t>{0, count - 1}(rng);
    }

    limit max_idle_iterations_;
    limit max_iterations_;
    limit max_evaluations_;
    TabuList tabu_list_;
    EASYLOCAL_NO_UNIQUE_ADDRESS Aspiration aspiration_;
};

} // namespace detail

/// Tabu Search exploring the whole neighborhood: the best admissible move is
/// applied, ties broken uniformly at random.
///
/// When every move is tabu, the least tabu one is applied. It stops after
/// max_idle_iterations iterations without improving the best cost, and returns
/// the best solution found. Requires a neighborhood explorer that enumerates
/// its moves (moves(), or a cursor), a cost with better(), and what the tabu
/// list needs: the neighborhood's inverse() for most lists (see
/// easylocal::runners::tabu).
template<class TabuList = tabu::FixedLength, class Aspiration = aspiration::ByObjective>
class TabuSearch
{
public:
    /// The parameter block of the algorithm.
    using parameters_type = TabuSearchParameters<typename TabuList::parameters_type>;

    /// From its parameters and an aspiration criterion.
    ///
    /// Throws `std::invalid_argument` when the parameters are not valid.
    explicit TabuSearch(const parameters_type& parameters, Aspiration aspiration = {})
        : engine_{config::require_valid(parameters), std::move(aspiration)}
    {
    }

    /// Runs the search from solution, with rng for the ties and the random
    /// choices of the list.
    ///
    /// The bound runner calls it, with the run of its context (neighborhood,
    /// evaluation, cost relations).
    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::tabu_search_context<typename Run::context_type>
        && tabu_list_for<TabuList, Run, RNG> && aspiration_for<Aspiration, Run>
        && detail::tabu_escape_supported<TabuList, Run, RNG>
    [[nodiscard]]
    auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
    {
        return engine_.search(run, std::move(solution), rng, [](const auto&...) {
            return [](const auto&...) { return false; };
        });
    }

private:
    detail::tabu_search_engine<TabuList, Aspiration> engine_;
};

/// Tabu Search stopping the scan at the first admissible move that improves the
/// current cost or, with improve_on_best (the group candidates,
/// candidates::FirstImprovementParameters), the best cost; without one, the
/// best admissible move of the whole neighborhood is applied, as in TabuSearch.
///
/// Requires what TabuSearch requires.
template<class TabuList = tabu::FixedLength, class Aspiration = aspiration::ByObjective>
class FirstImprovementTabuSearch
{
public:
    /// The parameter block of the algorithm.
    using parameters_type = TabuSearchParameters<
        typename TabuList::parameters_type,
        candidates::FirstImprovementParameters>;

    /// From its parameters and an aspiration criterion.
    ///
    /// Throws `std::invalid_argument` when the parameters are not valid.
    explicit FirstImprovementTabuSearch(
        const parameters_type& parameters,
        Aspiration aspiration = {})
        : engine_{config::require_valid(parameters), std::move(aspiration)},
          improve_on_best_{parameters.candidates.improve_on_best}
    {
    }

    /// Runs the search from solution, with rng for the ties and the random
    /// choices of the list.
    ///
    /// The bound runner calls it, with the run of its context (neighborhood,
    /// evaluation, cost relations).
    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::tabu_search_context<typename Run::context_type>
        && tabu_list_for<TabuList, Run, RNG> && aspiration_for<Aspiration, Run>
        && detail::tabu_escape_supported<TabuList, Run, RNG>
    [[nodiscard]]
    auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
    {
        return engine_.search(
            run,
            std::move(solution),
            rng,
            [improve_on_best = improve_on_best_](const auto&, const auto&) {
                return
                    [improve_on_best](
                        const Run& search,
                        const auto& candidate,
                        const auto& current,
                        const auto& best) {
                        return search.better(candidate, improve_on_best ? best : current);
                    };
            });
    }

private:
    detail::tabu_search_engine<TabuList, Aspiration> engine_;
    bool improve_on_best_;
};

/// Tabu Search with Glover's aspiration plus candidate strategy: each scan
/// examines admissible moves until plus more after the first one whose cost is
/// under the aspiration level, but at least min_moves and at most max_moves of
/// them, and applies the best of those examined (the group candidates,
/// candidates::AspirationPlusParameters).
///
/// The aspiration level is best + (aspiration_level - 1) * |best|, for the best
/// cost best: aspiration_level times it when it is positive, and never below
/// it. It is a value of the cost, so the cost is arithmetic. Requires what
/// TabuSearch requires, with an arithmetic cost.
template<class TabuList = tabu::FixedLength, class Aspiration = aspiration::ByObjective>
class AspirationPlusTabuSearch
{
public:
    /// The parameter block of the algorithm.
    using parameters_type = TabuSearchParameters<
        typename TabuList::parameters_type,
        candidates::AspirationPlusParameters>;

    /// From its parameters and an aspiration criterion.
    ///
    /// Throws `std::invalid_argument` when the parameters are not valid.
    explicit AspirationPlusTabuSearch(
        const parameters_type& parameters,
        Aspiration aspiration = {})
        : engine_{config::require_valid(parameters), std::move(aspiration)},
          min_moves_{parameters.candidates.min_moves},
          max_moves_{parameters.candidates.max_moves},
          plus_{parameters.candidates.plus},
          aspiration_level_{parameters.candidates.aspiration_level}
    {
    }

    /// Rejects, with a readable message, a cost that is not arithmetic.
    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::tabu_search_context<typename Run::context_type>
        && (!cost::arithmetic<typename Run::cost_type>)
    [[nodiscard]]
    typename Run::result_type run(Run&, typename Run::solution_type, RNG&) const
    {
        static_assert(
            easylocal::detail::always_false_v<Run>,
            "Aspiration plus Tabu Search compares the costs with the aspiration level, a value of the cost: it "
            "requires an arithmetic cost (cost::arithmetic), not a hierarchical, "
            "lexicographic or Pareto one");
        std::unreachable();
    }

    /// Runs the search from solution, with rng for the ties and the random
    /// choices of the list.
    ///
    /// The bound runner calls it, with the run of its context (neighborhood,
    /// evaluation, cost relations).
    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::tabu_search_context<typename Run::context_type>
        && cost::arithmetic<typename Run::cost_type> && tabu_list_for<TabuList, Run, RNG>
        && aspiration_for<Aspiration, Run>
        && detail::tabu_escape_supported<TabuList, Run, RNG>
    [[nodiscard]]
    auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
    {
        return engine_.search(
            run,
            std::move(solution),
            rng,
            [this](const Run&, const typename Run::cost_type& best) {
                const auto level =
                    detail::relaxed_level(static_cast<double>(best), aspiration_level_);
                return
                    [this, level, examined = std::size_t{0}, first = std::size_t{0}](
                        const Run&,
                        const typename Run::cost_type& candidate,
                        const auto&,
                        const auto&) mutable {
                        ++examined;
                        if (first == 0 && static_cast<double>(candidate) < level)
                            first = examined;
                        return examined >= max_moves_
                            || (first != 0 && examined - first >= plus_
                                && examined >= min_moves_);
                    };
            });
    }

private:
    detail::tabu_search_engine<TabuList, Aspiration> engine_;
    std::size_t min_moves_;
    std::size_t max_moves_;
    std::size_t plus_;
    double aspiration_level_;
};

/// Tabu Search with Glover's elite candidate list: a full scan applies the best
/// admissible move and keeps the elite_size best other admissible moves; the
/// following iterations evaluate only the kept moves still valid, and apply the
/// best admissible one while its cost is not above the quality level (the group
/// candidates, candidates::EliteListParameters).
///
/// Otherwise a new full scan builds a new list. The quality level is best +
/// (quality - 1) * |best|, for the best cost best: quality times it when it is
/// positive, and never below it. It is a value of the cost, so the cost is
/// arithmetic. Requires what TabuSearch requires, with an arithmetic cost.
template<class TabuList = tabu::FixedLength, class Aspiration = aspiration::ByObjective>
class EliteCandidateTabuSearch
{
public:
    /// The parameter block of the algorithm.
    using parameters_type = TabuSearchParameters<
        typename TabuList::parameters_type,
        candidates::EliteListParameters>;

    /// From its parameters and an aspiration criterion.
    ///
    /// Throws `std::invalid_argument` when the parameters are not valid.
    explicit EliteCandidateTabuSearch(
        const parameters_type& parameters,
        Aspiration aspiration = {})
        : engine_{config::require_valid(parameters), std::move(aspiration)},
          elite_size_{parameters.candidates.elite_size},
          quality_{parameters.candidates.quality}
    {
    }

    /// Rejects, with a readable message, a cost that is not arithmetic.
    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::tabu_search_context<typename Run::context_type>
        && (!cost::arithmetic<typename Run::cost_type>)
    [[nodiscard]]
    typename Run::result_type run(Run&, typename Run::solution_type, RNG&) const
    {
        static_assert(
            easylocal::detail::always_false_v<Run>,
            "Elite candidate Tabu Search compares the costs with the quality level, a value of the cost: it "
            "requires an arithmetic cost (cost::arithmetic), not a hierarchical, "
            "lexicographic or Pareto one");
        std::unreachable();
    }

    /// Runs the search from solution, with rng for the ties and the random
    /// choices of the list.
    ///
    /// The bound runner calls it, with the run of its context (neighborhood,
    /// evaluation, cost relations).
    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::tabu_search_context<typename Run::context_type>
        && cost::arithmetic<typename Run::cost_type> && tabu_list_for<TabuList, Run, RNG>
        && aspiration_for<Aspiration, Run>
        && detail::tabu_escape_supported<TabuList, Run, RNG>
    [[nodiscard]]
    auto run(Run& run, typename Run::solution_type solution, RNG& rng) const
    {
        using move_type = typename Run::move_type;
        using cost_type = typename Run::cost_type;
        const auto never = [](const auto&...) { return false; };

        auto state = engine_.start(run, std::move(solution));
        std::vector<move_type> elite;
        while (!run.should_stop())
        {
            if (const auto reason = engine_.limit_reached(run, state))
                return engine_.finish(run, state, *reason);
            const auto level =
                detail::relaxed_level(static_cast<double>(state.best.cost), quality_);

            // The kept moves still valid, while the best is good enough.
            std::erase_if(elite, [&](const move_type& move) {
                return !run.neighborhood_explorer().is_valid(state.solution, move);
            });
            if (!elite.empty())
            {
                auto kept = engine_.scan(run, state, elite, never, rng, never);
                if (kept.interrupted)
                    break;
                if (kept.chosen.has_value()
                    && static_cast<double>(kept.chosen->cost()) <= level)
                {
                    elite.erase(
                        elite.begin()
                        + static_cast<std::ptrdiff_t>(kept.chosen_position));
                    if (!engine_.apply(run, state, std::move(kept), rng))
                        break;
                    continue;
                }
            }

            // A full scan, keeping the best admissible moves.
            std::vector<std::tuple<move_type, cost_type, std::size_t>> best_moves;
            auto keep =
                [&](const move_type& move,
                    const cost_type& cost,
                    const std::size_t position) {
                    if (best_moves.size() < elite_size_ + 1)
                    {
                        best_moves.emplace_back(move, cost, position);
                        return;
                    }
                    const auto worst = std::ranges::max_element(
                        best_moves,
                        [&run](const auto& lhs, const auto& rhs) {
                            return run.better(std::get<1>(lhs), std::get<1>(rhs));
                        });
                    if (run.better(cost, std::get<1>(*worst)))
                        *worst = std::tuple<move_type, cost_type, std::size_t>{
                            move,
                            cost,
                            position};
                };
            auto full =
                engine_.scan(run, state, run.moves(state.solution), never, rng, keep);
            if (full.interrupted)
                break;
            if (full.empty)
                return engine_.finish(run, state, termination_reason::local_optimum);
            // Best first, so that one move too many is the worst.
            std::ranges::sort(best_moves, [&run](const auto& lhs, const auto& rhs) {
                return run.better(std::get<1>(lhs), std::get<1>(rhs));
            });
            elite.clear();
            for (const auto& [move, cost, position] : best_moves)
                if (!full.chosen.has_value() || position != full.chosen_position)
                    elite.push_back(move);
            if (elite.size() > elite_size_)
                elite.pop_back();
            if (!engine_.apply(run, state, std::move(full), rng))
                break;
        }
        return engine_.finish(run, state);
    }

private:
    detail::tabu_search_engine<TabuList, Aspiration> engine_;
    std::size_t elite_size_;
    double quality_;
};

} // namespace easylocal::runners
