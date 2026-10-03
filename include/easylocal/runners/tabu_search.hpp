#pragma once

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost/concepts.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/runners/detail/context_concepts.hpp>
#include <easylocal/runners/search_run.hpp>
#include <easylocal/utils/detail/attributes.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <random>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// Tabu Search: at each iteration the best admissible move of the neighborhood
// is applied, even when it worsens the cost. A tabu list forbids the moves that
// would undo recent ones (by the neighborhood's inverse), unless an aspiration
// criterion lifts the prohibition. The tabu lists live in runners::tabu, the
// aspiration criteria in runners::aspiration.
namespace easylocal::runners
{

// A candidate move, as a tabu list sees it: forbidden_by(tabu_move), whether a
// move the list holds forbids it (the neighborhood's inverse), its attribute()
// when the neighborhood has one, and its cost() for the lists that need it
// (their state declares needs_cost = true).
template<class Run>
class tabu_candidate
{
public:
    using move_type = typename Run::move_type;
    using solution_type = typename Run::solution_type;
    using cost_type = typename Run::cost_type;
    using neighborhood_type = typename Run::neighborhood_explorer_type;

    tabu_candidate(
        const Run& run,
        const solution_type& solution,
        const move_type& move,
        const cost_type* cost = nullptr) noexcept
        : run_{run}, solution_{solution}, move_{move}, cost_{cost}
    {
    }

    // The cost after the move; only for lists with needs_cost.
    [[nodiscard]]
    const cost_type& cost() const noexcept
    {
        assert(cost_ != nullptr && "the list's state must declare needs_cost = true");
        return *cost_;
    }

    [[nodiscard]]
    const move_type& move() const noexcept
    {
        return move_;
    }

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

    [[nodiscard]]
    auto attribute() const
        requires has_tabu_attribute<neighborhood_type>
    {
        return easylocal::tabu_attribute(run_.neighborhood_explorer(), move_);
    }

private:
    const Run& run_;
    const solution_type& solution_;
    const move_type& move_;
    const cost_type* cost_;
};

// What a tabu list learns after each iteration: the move applied, the solution
// and its cost after it, the iteration (counted from 1), whether the move
// improved the best cost, the move's attribute() and the solution_hash() when
// the problem has them.
template<class Run>
class tabu_step
{
public:
    using move_type = typename Run::move_type;
    using solution_type = typename Run::solution_type;
    using cost_type = typename Run::cost_type;
    using neighborhood_type = typename Run::neighborhood_explorer_type;

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

    [[nodiscard]]
    const move_type& move() const noexcept
    {
        return move_;
    }

    [[nodiscard]]
    const solution_type& solution() const noexcept
    {
        return solution_;
    }

    [[nodiscard]]
    const cost_type& cost() const noexcept
    {
        return cost_;
    }

    [[nodiscard]]
    std::size_t iteration() const noexcept
    {
        return run_.iterations();
    }

    [[nodiscard]]
    bool improved_best() const noexcept
    {
        return improved_best_;
    }

    [[nodiscard]]
    auto attribute() const
        requires has_tabu_attribute<neighborhood_type>
    {
        return easylocal::tabu_attribute(run_.neighborhood_explorer(), move_);
    }

    template<class R = Run>
        requires has_solution_hash<
            std::remove_cvref_t<decltype(std::declval<const R&>().solution_manager())>>
    [[nodiscard]]
    std::uint64_t solution_hash() const
    {
        return easylocal::solution_hash(run_.solution_manager(), solution_);
    }

private:
    const Run& run_;
    const move_type& move_;
    const solution_type& solution_;
    const cost_type& cost_;
    bool improved_best_;
};

// A tabu list policy: a value holding its parameters, from which each run makes
// the list's state with make_state<Run>(). The state answers
// tabu_tenure(candidate): the iterations left before the candidate is no longer
// tabu, or nothing when it is admissible; update(step, rng) records an applied
// move. A state may also have escape_moves(): a number of random moves to apply
// at once, then reset to 0 (the reactive list's escape).
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
        const tabu_candidate<Run>& candidate,
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
                tenure = tenure.has_value() ? std::max(*tenure, left) : left;
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
// than the current length.
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

private:
    std::deque<std::pair<Move, std::size_t>> moves_;
};

} // namespace detail

struct FixedLengthParameters
{
    // Iterations a move stays in the list.
    std::size_t tenure{10};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"tenure", &FixedLengthParameters::tenure>(
                "Number of iterations a move stays tabu"));
    }

    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        if (tenure == 0)
            return config::validation_result::failure("tenure must be positive");
        return config::validation_result::success();
    }
};

// TS1: a list of the last tenure moves, overwritten in a circle: each move
// stays tabu for tenure iterations.
class FixedLength
{
public:
    using parameters_type = FixedLengthParameters;

    explicit FixedLength(const FixedLengthParameters& parameters) noexcept
        : parameters_{parameters}
    {
        assert(parameters_.validate());
    }

    template<class Move>
    class state
    {
    public:
        explicit state(const std::size_t tenure) : tenure_{tenure}
        {
            moves_.reserve(tenure);
        }

        // A move forbidden by several entries is tabu until the youngest
        // expires.
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

    private:
        std::vector<Move> moves_;
        std::size_t newest_{};
        std::size_t tenure_;
    };

    template<class Run>
    [[nodiscard]]
    state<typename Run::move_type> make_state() const
    {
        return state<typename Run::move_type>{parameters_.tenure};
    }

private:
    FixedLengthParameters parameters_;
};

struct RandomTenureParameters
{
    std::size_t min_tenure{5};
    std::size_t max_tenure{15};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"min_tenure", &RandomTenureParameters::min_tenure>(
                "Minimum number of iterations a move stays tabu"),
            config::field<"max_tenure", &RandomTenureParameters::max_tenure>(
                "Maximum number of iterations a move stays tabu"));
    }

    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        if (min_tenure == 0)
            return config::validation_result::failure("min_tenure must be positive");
        if (max_tenure < min_tenure)
            return config::validation_result::failure(
                "max_tenure must not be below min_tenure");
        return config::validation_result::success();
    }
};

// TS2 (Gendreau, Hertz and Laporte): each move stays tabu for a tenure drawn
// uniformly in [min_tenure, max_tenure].
class RandomTenure
{
public:
    using parameters_type = RandomTenureParameters;

    explicit RandomTenure(const RandomTenureParameters& parameters) noexcept
        : parameters_{parameters}
    {
        assert(parameters_.validate());
    }

    template<class Move>
    class state
    {
    public:
        explicit state(const RandomTenureParameters& parameters) : parameters_{parameters}
        {
        }

        template<class Candidate>
            requires detail::inverse_candidate<Candidate, Move>
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
        {
            return moves_.tabu_tenure(candidate, iteration_);
        }

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

    template<class Run>
    [[nodiscard]]
    state<typename Run::move_type> make_state() const
    {
        return state<typename Run::move_type>{parameters_};
    }

private:
    RandomTenureParameters parameters_;
};

struct CyclicParameters
{
    // Iterations each tenure is used for.
    std::size_t period{100};
    // The tenures, used in turn.
    std::vector<std::size_t> tenures{11, 34, 20, 8, 98};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"period", &CyclicParameters::period>(
                "Number of iterations each tenure is used for"),
            config::field<"tenures", &CyclicParameters::tenures>(
                "The tenures, used in turn"));
    }

    [[nodiscard]]
    config::validation_result validate() const
    {
        if (period == 0)
            return config::validation_result::failure("period must be positive");
        if (tenures.empty())
            return config::validation_result::failure("tenures must not be empty");
        if (std::ranges::find(tenures, std::size_t{0}) != tenures.end())
            return config::validation_result::failure("tenures must be positive");
        return config::validation_result::success();
    }
};

// TS3 (Taillard): the tenure of the moves changes every period iterations,
// taking the given tenures in turn.
class Cyclic
{
public:
    using parameters_type = CyclicParameters;

    explicit Cyclic(const CyclicParameters& parameters) : parameters_{parameters}
    {
        assert(parameters_.validate());
    }

    template<class Move>
    class state
    {
    public:
        explicit state(const CyclicParameters& parameters) : parameters_{parameters} {}

        template<class Candidate>
            requires detail::inverse_candidate<Candidate, Move>
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
        {
            return moves_.tabu_tenure(candidate, iteration_);
        }

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

        // The tenure the next moves get.
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

    template<class Run>
    [[nodiscard]]
    state<typename Run::move_type> make_state() const
    {
        return state<typename Run::move_type>{parameters_};
    }

private:
    CyclicParameters parameters_;
};

struct ReactiveParameters
{
    // Factor of the tenure when a solution comes back within cycle_length
    // iterations.
    double increase{1.1};
    // Factor of the tenure when no cycle is seen for longer than the average
    // cycle length.
    double decrease{0.9};
    // Visits of a solution after which each further visit counts as chaos.
    std::size_t repetitions{3};
    // Chaos counts after which the search escapes with random moves.
    std::size_t chaos{3};
    // Revisits closer than this many iterations are cycles.
    std::size_t cycle_length{50};
    // The largest tenure.
    std::size_t max_tenure{1000};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"increase", &ReactiveParameters::increase>(
                "Factor of the tenure when a solution is revisited within cycle_length"),
            config::field<"decrease", &ReactiveParameters::decrease>(
                "Factor of the tenure when no cycle is seen for the average cycle length"),
            config::field<"repetitions", &ReactiveParameters::repetitions>(
                "Visits of a solution after which further visits count as chaos"),
            config::field<"chaos", &ReactiveParameters::chaos>(
                "Chaos counts after which the search escapes with random moves"),
            config::field<"cycle_length", &ReactiveParameters::cycle_length>(
                "Revisits closer than this many iterations are cycles"),
            config::field<"max_tenure", &ReactiveParameters::max_tenure>(
                "The largest tenure"));
    }

    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        if (!std::isfinite(increase) || increase <= 1.0)
            return config::validation_result::failure("increase must be greater than 1");
        if (!std::isfinite(decrease) || decrease <= 0.0 || decrease >= 1.0)
            return config::validation_result::failure(
                "decrease must be in the open interval (0, 1)");
        if (cycle_length == 0)
            return config::validation_result::failure("cycle_length must be positive");
        if (max_tenure == 0)
            return config::validation_result::failure("max_tenure must be positive");
        return config::validation_result::success();
    }
};

// TS4, Reactive Tabu Search (Battiti and Tecchiolli): the tenure starts at 1
// and reacts to the solutions visited, recognized by their hash. A solution
// revisited within cycle_length iterations multiplies the tenure by increase
// and updates the average cycle length; when no such cycle occurs for longer
// than the average, the tenure is multiplied by decrease. A solution visited
// more than repetitions times counts as chaos; after more than chaos counts the
// memory is reset and the search escapes with 1 + (1 + r) * average / 2
// random moves, r uniform in [0, 1). It needs the solution hash
// (has_solution_hash); a collision is taken for a revisit.
class Reactive
{
public:
    using parameters_type = ReactiveParameters;

    explicit Reactive(const ReactiveParameters& parameters) noexcept
        : parameters_{parameters}
    {
        assert(parameters_.validate());
    }

    template<class Move>
    class state
    {
    public:
        explicit state(const ReactiveParameters& parameters)
            : parameters_{parameters},
              average_cycle_{static_cast<double>(parameters.cycle_length)}
        {
        }

        template<class Candidate>
            requires detail::inverse_candidate<Candidate, Move>
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
        {
            const auto tenure = current_tenure();
            for (const auto& [move, applied] : moves_)
            {
                const auto age = iteration_ - applied;
                if (age >= tenure)
                    break;
                if (candidate.forbidden_by(move))
                    return tenure - age;
            }
            return std::nullopt;
        }

        template<class Step, class RNG>
            requires requires(const Step& step) {
                { step.solution_hash() } -> std::convertible_to<std::uint64_t>;
            }
        void update(const Step& step, RNG& rng)
        {
            iteration_ = step.iteration();
            moves_.emplace_front(step.move(), iteration_);
            ++since_change_;

            const auto [visit, first] =
                history_.try_emplace(step.solution_hash(), visit_record{iteration_, 1});
            if (!first)
            {
                const auto cycle = iteration_ - visit->second.last;
                visit->second.last = iteration_;
                if (++visit->second.count > parameters_.repetitions
                    && ++chaos_ > parameters_.chaos)
                {
                    std::uniform_real_distribution<double> draw{0.0, 1.0};
                    escape_ = 1
                        + static_cast<std::size_t>(
                            (1.0 + draw(rng)) * average_cycle_ / 2.0);
                    history_.clear();
                    moves_.clear();
                    chaos_ = 0;
                    tenure_ = 1.0;
                    since_change_ = 0;
                    return;
                }
                if (cycle < parameters_.cycle_length)
                {
                    tenure_ = std::min(
                        tenure_ * parameters_.increase,
                        static_cast<double>(parameters_.max_tenure));
                    average_cycle_ =
                        0.9 * average_cycle_ + 0.1 * static_cast<double>(cycle);
                    since_change_ = 0;
                }
            }
            if (static_cast<double>(since_change_) > average_cycle_)
            {
                tenure_ = std::max(1.0, tenure_ * parameters_.decrease);
                since_change_ = 0;
            }

            const auto tenure = current_tenure();
            while (!moves_.empty() && iteration_ - moves_.back().second >= tenure)
                moves_.pop_back();
        }

        [[nodiscard]]
        std::size_t escape_moves() noexcept
        {
            return std::exchange(escape_, 0);
        }

        // The iterations a move applied now stays tabu.
        [[nodiscard]]
        std::size_t current_tenure() const noexcept
        {
            return std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(tenure_)));
        }

    private:
        struct visit_record
        {
            std::size_t last;
            std::size_t count;
        };

        ReactiveParameters parameters_;
        // Newest first.
        std::deque<std::pair<Move, std::size_t>> moves_;
        std::unordered_map<std::uint64_t, visit_record> history_;
        double tenure_{1.0};
        double average_cycle_;
        std::size_t since_change_{};
        std::size_t chaos_{};
        std::size_t escape_{};
        std::size_t iteration_{};
    };

    template<class Run>
    [[nodiscard]]
    state<typename Run::move_type> make_state() const
    {
        return state<typename Run::move_type>{parameters_};
    }

private:
    ReactiveParameters parameters_;
};

struct FrequencyParameters
{
    // Relative frequency above which an attribute is tabu.
    double threshold{0.05};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"threshold", &FrequencyParameters::threshold>(
                "Relative frequency of an attribute above which its moves are tabu"));
    }

    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        if (!std::isfinite(threshold) || threshold <= 0.0 || threshold > 1.0)
            return config::validation_result::failure(
                "threshold must be in the interval (0, 1]");
        return config::validation_result::success();
    }
};

// TS5, frequency-based long-term memory (Glover and Laguna's transition
// measure): a move is tabu while the attribute of its moves (tabu_attribute)
// has been applied in more than threshold of the iterations so far. Its tenure
// is the iterations before that frequency falls to the threshold. It needs the
// neighborhood's tabu_attribute, not its inverse.
class Frequency
{
public:
    using parameters_type = FrequencyParameters;

    explicit Frequency(const FrequencyParameters& parameters) noexcept
        : parameters_{parameters}
    {
        assert(parameters_.validate());
    }

    template<class Attribute>
    class state
    {
    public:
        explicit state(const FrequencyParameters& parameters) : parameters_{parameters} {}

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

struct ObjectiveBasedParameters
{
    // Iterations a cost value stays tabu.
    std::size_t tenure{10};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"tenure", &ObjectiveBasedParameters::tenure>(
                "Number of iterations a reached cost stays tabu"));
    }

    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        if (tenure == 0)
            return config::validation_result::failure("tenure must be positive");
        return config::validation_result::success();
    }
};

// Tabu on cost values (Gendreau and Potvin): a move is tabu when it would
// reach a cost equal to one reached in the last tenure iterations. It needs
// neither inverse nor attribute, but the candidate's cost: moves are evaluated
// before the tabu check.
class ObjectiveBased
{
public:
    using parameters_type = ObjectiveBasedParameters;

    explicit ObjectiveBased(const ObjectiveBasedParameters& parameters) noexcept
        : parameters_{parameters}
    {
        assert(parameters_.validate());
    }

    template<class Cost>
    class state
    {
    public:
        static constexpr bool needs_cost = true;

        explicit state(const std::size_t tenure) : tenure_{tenure} {}

        template<class Candidate>
            requires requires(const Candidate& candidate) { candidate.cost(); }
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
        {
            for (std::size_t age = 0; age < costs_.size(); ++age)
                if (costs_[age] == candidate.cost())
                    return tenure_ - age;
            return std::nullopt;
        }

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

    template<class Run>
        requires std::equality_comparable<typename Run::cost_type>
    [[nodiscard]]
    state<typename Run::cost_type> make_state() const
    {
        return state<typename Run::cost_type>{parameters_.tenure};
    }

private:
    ObjectiveBasedParameters parameters_;
};

struct LimDynamicParameters
{
    std::size_t min_tenure{5};
    std::size_t max_tenure{20};
    // Iterations without improving the best cost after which the tenure grows.
    std::size_t idle_threshold{10};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"min_tenure", &LimDynamicParameters::min_tenure>(
                "The tenure after an improvement of the best cost"),
            config::field<"max_tenure", &LimDynamicParameters::max_tenure>(
                "The tenure at which it falls back to min_tenure"),
            config::field<"idle_threshold", &LimDynamicParameters::idle_threshold>(
                "Iterations without improvement after which the tenure grows"));
    }

    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        if (min_tenure == 0)
            return config::validation_result::failure("min_tenure must be positive");
        if (max_tenure <= min_tenure)
            return config::validation_result::failure(
                "max_tenure must exceed min_tenure");
        return config::validation_result::success();
    }
};

// A tenure that grows by one at each iteration after idle_threshold iterations
// without improving the best cost, and falls back to min_tenure when the best
// improves or the tenure reaches max_tenure.
class LimDynamic
{
public:
    using parameters_type = LimDynamicParameters;

    explicit LimDynamic(const LimDynamicParameters& parameters) noexcept
        : parameters_{parameters}
    {
        assert(parameters_.validate());
    }

    template<class Move>
    class state
    {
    public:
        explicit state(const LimDynamicParameters& parameters)
            : parameters_{parameters}, tenure_{parameters.min_tenure}
        {
        }

        template<class Candidate>
            requires detail::inverse_candidate<Candidate, Move>
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
        {
            return moves_.tabu_tenure(candidate, iteration_, tenure_);
        }

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
// least) otherwise. The spread is cost::delta(highest, lowest).
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
        moves_.trim(iteration_, tenure_);
        return window_over;
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
    std::size_t tenure_{1};
    std::size_t in_window_{};
    std::size_t iteration_{};
};

} // namespace detail

struct FooParameters
{
    // Iterations between two tenure changes.
    std::size_t window{100};
    // Growth of the tenure when the costs fluctuate little; also the initial
    // tenure.
    std::size_t increment{5};
    // The spread of the costs in a window below which the tenure grows.
    double fluctuation{1.0};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"window", &FooParameters::window>(
                "Iterations between two tenure changes"),
            config::field<"increment", &FooParameters::increment>(
                "Growth of the tenure when the costs fluctuate little (the initial tenure)"),
            config::field<"fluctuation", &FooParameters::fluctuation>(
                "Spread of the costs in a window below which the tenure grows"));
    }

    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        if (window == 0)
            return config::validation_result::failure("window must be positive");
        if (increment == 0)
            return config::validation_result::failure("increment must be positive");
        if (!std::isfinite(fluctuation) || fluctuation < 0.0)
            return config::validation_result::failure(
                "fluctuation must be finite and non-negative");
        return config::validation_result::success();
    }
};

// The Fluctuation Of the Objective scheme (Blöchliger and Zufferey): a tenure
// that grows by increment when the costs reached in the last window spread
// less than fluctuation (the search is stuck), and shrinks by one otherwise.
// The fluctuation is in cost units, so it depends on the instance; the cost
// needs cost::delta.
class Foo
{
public:
    using parameters_type = FooParameters;

    explicit Foo(const FooParameters& parameters) noexcept : parameters_{parameters}
    {
        assert(parameters_.validate());
    }

    template<class Move, class Cost>
    class state
    {
    public:
        explicit state(const FooParameters& parameters) : parameters_{parameters}
        {
            tenure_.set_tenure(parameters.increment);
        }

        template<class Candidate>
            requires detail::inverse_candidate<Candidate, Move>
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
        {
            return tenure_.tabu_tenure(candidate);
        }

        template<class Step, class RNG>
        void update(const Step& step, RNG&)
        {
            tenure_.update(
                step,
                parameters_.window,
                parameters_.increment,
                parameters_.fluctuation);
        }

        [[nodiscard]]
        std::size_t current_tenure() const noexcept
        {
            return tenure_.current_tenure();
        }

    private:
        FooParameters parameters_;
        detail::fluctuation_tenure<Move, Cost> tenure_;
    };

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

struct RandomFooParameters
{
    std::size_t min_window{50};
    std::size_t max_window{150};
    std::size_t min_increment{2};
    std::size_t max_increment{8};
    double min_fluctuation{0.5};
    double max_fluctuation{2.0};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<"min_window", &RandomFooParameters::min_window>(
                "Smallest window"),
            config::field<"max_window", &RandomFooParameters::max_window>(
                "Largest window"),
            config::field<"min_increment", &RandomFooParameters::min_increment>(
                "Smallest increment"),
            config::field<"max_increment", &RandomFooParameters::max_increment>(
                "Largest increment"),
            config::field<"min_fluctuation", &RandomFooParameters::min_fluctuation>(
                "Smallest fluctuation threshold"),
            config::field<"max_fluctuation", &RandomFooParameters::max_fluctuation>(
                "Largest fluctuation threshold"));
    }

    [[nodiscard]]
    config::validation_result validate() const noexcept
    {
        if (min_window == 0 || max_window < min_window)
            return config::validation_result::failure(
                "the windows must be positive, min_window not above max_window");
        if (min_increment == 0 || max_increment < min_increment)
            return config::validation_result::failure(
                "the increments must be positive, min_increment not above max_increment");
        if (!std::isfinite(min_fluctuation) || !std::isfinite(max_fluctuation)
            || min_fluctuation < 0.0 || max_fluctuation < min_fluctuation)
        {
            return config::validation_result::failure(
                "the fluctuations must be finite and non-negative, min_fluctuation not above "
                "max_fluctuation");
        }
        return config::validation_result::success();
    }
};

// Foo with its window, increment and fluctuation drawn uniformly in their
// ranges at the start and again at the end of each window.
class RandomFoo
{
public:
    using parameters_type = RandomFooParameters;

    explicit RandomFoo(const RandomFooParameters& parameters) noexcept
        : parameters_{parameters}
    {
        assert(parameters_.validate());
    }

    template<class Move, class Cost>
    class state
    {
    public:
        explicit state(const RandomFooParameters& parameters) : parameters_{parameters} {}

        template<class Candidate>
            requires detail::inverse_candidate<Candidate, Move>
        [[nodiscard]]
        std::optional<std::size_t> tabu_tenure(const Candidate& candidate) const
        {
            return tenure_.tabu_tenure(candidate);
        }

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
        }

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

namespace aspiration
{

// A tabu move is admitted when it would improve the best cost found.
struct ByObjective
{
    static constexpr bool needs_cost = true;

    template<class Run, class Cost>
    [[nodiscard]]
    bool overrides(const Run& run, const Cost& candidate, const Cost& best) const
    {
        return run.better(candidate, best);
    }
};

// Tabu moves are never admitted, and need not be evaluated.
struct None
{
    static constexpr bool needs_cost = false;

    template<class Run, class Cost>
    [[nodiscard]]
    bool overrides(const Run&, const Cost&, const Cost&) const
    {
        return false;
    }
};

} // namespace aspiration

template<class ListParameters>
struct TabuSearchParameters
{
    // Iterations without improving the best cost after which the search stops.
    std::size_t max_idle_iterations{1000};
    // 0: no limit.
    std::size_t max_iterations{0};
    // Evaluation budget, including the initial evaluation; 0 means no budget.
    std::size_t max_evaluations{0};
    ListParameters tabu_list{};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "max_idle_iterations",
                &TabuSearchParameters::max_idle_iterations>(
                "Maximum number of iterations without improving the best cost"),
            config::field<"max_iterations", &TabuSearchParameters::max_iterations>(
                "Maximum number of iterations (0: no limit)"),
            config::field<"max_evaluations", &TabuSearchParameters::max_evaluations>(
                "Maximum number of solution evaluations (0: no budget)"),
            config::group<"tabu_list", &TabuSearchParameters::tabu_list>(
                "The tabu list"));
    }

    // The tabu list is validated as a group.
    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        if (max_idle_iterations == 0)
        {
            return config::validation_result::failure(
                "max_idle_iterations must be positive");
        }
        return config::validation_result::success();
    }
};

template<class ListParameters>
struct FirstImprovementTabuSearchParameters
{
    std::size_t max_idle_iterations{1000};
    std::size_t max_iterations{0};
    std::size_t max_evaluations{0};
    // The scan stops at the first admissible move that improves the best cost
    // rather than the current one.
    bool improve_on_best{false};
    ListParameters tabu_list{};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "max_idle_iterations",
                &FirstImprovementTabuSearchParameters::max_idle_iterations>(
                "Maximum number of iterations without improving the best cost"),
            config::field<
                "max_iterations",
                &FirstImprovementTabuSearchParameters::max_iterations>(
                "Maximum number of iterations (0: no limit)"),
            config::field<
                "max_evaluations",
                &FirstImprovementTabuSearchParameters::max_evaluations>(
                "Maximum number of solution evaluations (0: no budget)"),
            config::field<
                "improve_on_best",
                &FirstImprovementTabuSearchParameters::improve_on_best>(
                "Stop the scan at a move improving the best cost, not the current one"),
            config::group<"tabu_list", &FirstImprovementTabuSearchParameters::tabu_list>(
                "The tabu list"));
    }

    [[nodiscard]]
    constexpr config::validation_result validate() const noexcept
    {
        if (max_idle_iterations == 0)
        {
            return config::validation_result::failure(
                "max_idle_iterations must be positive");
        }
        return config::validation_result::success();
    }
};

namespace detail
{

template<class Context>
concept tabu_search_context = enumerating_strict_improvement_context<Context>;

// A list that escapes with random moves needs a neighborhood that draws them.
template<class List, class Run, class RNG>
concept tabu_escape_supported =
    !requires(decltype(std::declval<const List&>().template make_state<Run>())& state) {
        state.escape_moves();
    } || random_move_context<typename Run::context_type, RNG>;

// The machinery common to the tabu searches; StopAt decides, for each
// admissible candidate, whether the scan stops at it.
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

    // make_stop(run, best_cost) gives, for each scan, the callable that
    // decides at each admissible candidate whether the scan stops there.
    template<class Run, class RNG, class MakeStop>
    [[nodiscard]]
    auto search(
        Run& run,
        typename Run::solution_type solution,
        RNG& rng,
        MakeStop make_stop) const
    {
        using move_type = typename Run::move_type;
        using cost_type = typename Run::cost_type;

        if (max_evaluations_ != 0)
            run.limit_evaluations(max_evaluations_);
        auto current = run.start(solution);
        auto best_solution = solution;
        auto best_cost = current.cost();
        auto list = tabu_list_.template make_state<Run>();
        constexpr bool list_needs_cost = requires {
            requires std::remove_cvref_t<decltype(list)>::needs_cost;
        };
        std::size_t idle_iterations = 0;

        while (!run.should_stop())
        {
            if (idle_iterations >= max_idle_iterations_)
            {
                return run.finish(
                    std::move(best_solution),
                    std::move(best_cost),
                    termination_reason::idle_limit_reached);
            }
            if (max_iterations_ != 0 && run.iterations() >= max_iterations_)
                return run.finish(std::move(best_solution), std::move(best_cost));

            // The chosen admissible candidate; ties share the choice
            // uniformly (each of k equivalent candidates replaces the choice
            // with probability 1/k).
            std::optional<typename Run::candidate_type> chosen;
            std::optional<move_type> chosen_move;
            std::size_t ties = 0;
            // The least tabu move, applied when no move is admissible.
            std::optional<move_type> least_tabu;
            std::size_t least_tenure = 0;
            std::size_t least_ties = 0;
            bool neighborhood_empty = true;
            auto stop_at = make_stop(run, best_cost);

            for (const auto& move : run.moves(solution))
            {
                if (run.should_stop())
                    return run.finish(std::move(best_solution), std::move(best_cost));
                neighborhood_empty = false;

                // A list on costs sees the candidate evaluated.
                std::optional<typename Run::candidate_type> evaluated;
                if constexpr (list_needs_cost)
                    evaluated = run.evaluate_move(solution, current, move);
                const auto tenure = list.tabu_tenure(
                    tabu_candidate<Run>{
                        run,
                        solution,
                        move,
                        evaluated.has_value() ? &evaluated->cost() : nullptr});
                if (tenure.has_value())
                {
                    if (!least_tabu.has_value() || *tenure < least_tenure)
                    {
                        least_tabu = move;
                        least_tenure = *tenure;
                        least_ties = 1;
                    }
                    else if (*tenure == least_tenure && draw(rng, ++least_ties) == 0)
                    {
                        least_tabu = move;
                    }
                    if constexpr (!Aspiration::needs_cost)
                        continue;
                }

                auto candidate = evaluated.has_value()
                    ? std::move(*evaluated)
                    : run.evaluate_move(solution, current, move);
                if (tenure.has_value()
                    && !aspiration_.overrides(run, candidate.cost(), best_cost))
                {
                    continue;
                }

                const auto stop =
                    stop_at(run, candidate.cost(), current.cost(), best_cost);
                if (!chosen.has_value() || run.better(candidate.cost(), chosen->cost()))
                {
                    chosen = std::move(candidate);
                    chosen_move = move;
                    ties = 1;
                }
                else if (!run.better(chosen->cost(), candidate.cost())
                    && draw(rng, ++ties) == 0)
                {
                    chosen = std::move(candidate);
                    chosen_move = move;
                }
                if (stop)
                    break;
            }

            if (neighborhood_empty)
            {
                return run.finish(
                    std::move(best_solution),
                    std::move(best_cost),
                    termination_reason::local_optimum);
            }
            if (!chosen.has_value())
            {
                // Every move is tabu: the least tabu one is applied.
                assert(least_tabu.has_value());
                if (run.should_stop())
                    break;
                chosen = run.evaluate_move(solution, current, *least_tabu);
                chosen_move = std::move(least_tabu);
            }

            run.next_iteration();
            run.commit(solution, current, std::move(*chosen), *chosen_move);
            const bool improved = run.better(current.cost(), best_cost);
            if (improved)
            {
                const cost_type previous_best = best_cost;
                best_solution = solution;
                best_cost = current.cost();
                run.incumbent_updated(previous_best, best_cost);
                idle_iterations = 0;
            }
            else
            {
                ++idle_iterations;
            }
            list.update(
                tabu_step<Run>{run, *chosen_move, solution, current.cost(), improved},
                rng);

            // The reactive list's escape: random moves, applied whatever their
            // cost and not recorded in the list.
            if constexpr (requires { list.escape_moves(); })
            {
                for (auto escape = list.escape_moves(); escape > 0; --escape)
                {
                    if (run.should_stop()
                        || (max_iterations_ != 0 && run.iterations() >= max_iterations_))
                    {
                        break;
                    }
                    auto move = run.random_move(solution, rng);
                    if (!move.has_value())
                        break;
                    run.next_iteration();
                    auto candidate = run.evaluate_move(solution, current, *move);
                    run.commit(solution, current, std::move(candidate), *move);
                    if (run.better(current.cost(), best_cost))
                    {
                        const cost_type previous_best = best_cost;
                        best_solution = solution;
                        best_cost = current.cost();
                        run.incumbent_updated(previous_best, best_cost);
                        idle_iterations = 0;
                    }
                    else
                    {
                        ++idle_iterations;
                    }
                }
            }
        }

        return run.finish(std::move(best_solution), std::move(best_cost));
    }

private:
    // Uniform in [0, count).
    template<class RNG>
    [[nodiscard]]
    static std::size_t draw(RNG& rng, const std::size_t count)
    {
        return std::uniform_int_distribution<std::size_t>{0, count - 1}(rng);
    }

    std::size_t max_idle_iterations_;
    std::size_t max_iterations_;
    std::size_t max_evaluations_;
    TabuList tabu_list_;
    EASYLOCAL_NO_UNIQUE_ADDRESS Aspiration aspiration_;
};

} // namespace detail

// Tabu Search exploring the whole neighborhood: the best admissible move is
// applied, ties broken uniformly at random. When every move is tabu, the least
// tabu one is applied. It stops after max_idle_iterations iterations without
// improving the best cost, and returns the best solution found.
template<class TabuList = tabu::FixedLength, class Aspiration = aspiration::ByObjective>
class TabuSearch
{
public:
    using parameters_type = TabuSearchParameters<typename TabuList::parameters_type>;

    explicit TabuSearch(const parameters_type& parameters, Aspiration aspiration = {})
        : engine_{parameters, std::move(aspiration)}
    {
        assert(parameters.validate());
    }

    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::tabu_search_context<typename Run::context_type>
        && tabu_list_for<TabuList, Run, RNG>
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

// Tabu Search stopping the scan at the first admissible move that improves the
// current cost or, with improve_on_best, the best cost; without one, the best
// admissible move of the whole neighborhood is applied, as in TabuSearch.
template<class TabuList = tabu::FixedLength, class Aspiration = aspiration::ByObjective>
class FirstImprovementTabuSearch
{
public:
    using parameters_type =
        FirstImprovementTabuSearchParameters<typename TabuList::parameters_type>;

    explicit FirstImprovementTabuSearch(
        const parameters_type& parameters,
        Aspiration aspiration = {})
        : engine_{parameters, std::move(aspiration)},
          improve_on_best_{parameters.improve_on_best}
    {
        assert(parameters.validate());
    }

    template<class Run, std::uniform_random_bit_generator RNG>
        requires detail::tabu_search_context<typename Run::context_type>
        && tabu_list_for<TabuList, Run, RNG>
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

} // namespace easylocal::runners
