#pragma once

/// \file
/// Session: the state of an interactive session on an app (Input, current
/// solution, selected move, RNG) and the commands that change it.
///
/// It replaces EasyLocal 3's Tester; the TextUI and the REST adapter are views
/// on it.

#include <easylocal/app/check.hpp>
#include <easylocal/app/io.hpp>
#include <easylocal/app/run_parameters.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost/semantics.hpp>
#include <easylocal/cost/text.hpp>
#include <easylocal/helpers/detail/evaluation.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/solution_manager.hpp>

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <istream>
#include <memory>
#include <optional>
#include <ostream>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace easylocal
{

namespace detail
{

// Whether moves can be compared. std::variant (the move of a neighborhood
// union) declares == for any alternatives, so its alternatives are checked.
template<class Move>
inline constexpr bool comparable_moves_v = std::equality_comparable<Move>;

template<class... Moves>
inline constexpr bool comparable_moves_v<std::variant<Moves...>> =
    (comparable_moves_v<Moves> && ...);

// The optional members of a cost component for people: its name, and a text
// that explains its value on a solution, such as the violations it counts
// (EasyLocal 3's PrintViolations).
template<class Component>
concept named_component = requires(const Component& component) {
    { component.name() } -> std::convertible_to<std::string_view>;
};

template<class Component, class Solution>
concept describing_component =
    requires(const Component& component, const Solution& solution) {
        { component.describe(solution) } -> std::convertible_to<std::string>;
    };

// A cost or a component's value as text for reports (Session::cost_report,
// cli::run): as a cost reads it back when it is one, else by its describe
// hook or operator<<.
template<class Value>
[[nodiscard]]
std::string report_text(const Value& value)
{
    if constexpr (cost::text_readable<Value>)
        return cost::to_text(value);
    else if constexpr (describable<Value>)
        return easylocal::describe(value);
    else
        return "(not printable)";
}

// An app a Session can hold: movable, with the Input type of its problem. A
// named concept, not an inline requires-expression, which MrDocs would separate
// from the class and its comment.
template<class App>
concept session_app = std::move_constructible<App> && requires {
    typename App::input_type;
};

} // namespace detail

/// The state of an interactive session on an app, and the commands that change
/// it: an owned Input, the app bound to it, a current solution, a selected move
/// and an RNG.
///
/// It has no user interface of its own: interactive frontends, such as the
/// TextUI, are views on it. Requires a movable app, such as the one
/// easylocal::app() builds.
template<class App>
    requires detail::session_app<App>
class Session
{
public:
    /// The app of the session.
    using app_type = App;
    /// The Input of the problem.
    using input_type = typename App::input_type;
    /// The app bound to the Input, with its services.
    using bound_app_type =
        decltype(std::declval<const App&>().bind(std::declval<const input_type&>()));
    /// The SolutionManager of the bound app.
    using solution_manager_type = typename bound_app_type::solution_manager_type;
    /// The neighborhood explorer of the bound app.
    using neighborhood_type = typename bound_app_type::neighborhood_explorer_type;
    /// The Solution of the problem.
    using solution_type = typename solution_manager_type::solution_type;
    /// The cost of a solution.
    using cost_type = typename solution_manager_type::cost_type;
    /// The move of the neighborhood.
    using move_type = typename neighborhood_type::move_type;
    /// The random generator of the session.
    using rng_type = std::mt19937_64;

    /// Whether the SolutionManager has initial_solution().
    static constexpr bool supports_initial_solution =
        has_initial_solution<solution_manager_type>;
    /// Whether the SolutionManager has random_solution(rng).
    static constexpr bool supports_random_solution =
        has_random_solution<solution_manager_type, rng_type>;
    /// Whether the neighborhood enumerates its moves.
    static constexpr bool supports_deterministic_moves =
        deterministic_neighborhood_for<neighborhood_type, solution_type>;
    /// Whether the neighborhood draws random moves.
    static constexpr bool supports_random_moves =
        random_neighborhood_for<neighborhood_type, solution_type, rng_type>;
    /// Whether moves can be selected by their cost: the neighborhood enumerates
    /// them and the cost compares with better().
    static constexpr bool supports_improvement_selection =
        supports_deterministic_moves && cost::has_better<solution_manager_type>;
    /// Whether check_neighborhood_costs is available: the neighborhood
    /// enumerates its moves and the cost has equivalent().
    static constexpr bool supports_cost_consistency_check =
        supports_deterministic_moves && cost::has_equivalent<solution_manager_type>;
    /// Whether check_move_independence is available: the neighborhood
    /// enumerates its moves and solutions compare with `==`.
    static constexpr bool supports_move_independence_check =
        supports_deterministic_moves && std::equality_comparable<solution_type>;
    /// Whether check_random_move_distribution is available: the neighborhood
    /// enumerates and draws its moves, and moves compare with `==`.
    static constexpr bool supports_random_distribution_check =
        supports_deterministic_moves && supports_random_moves
        && detail::comparable_moves_v<move_type>;

    /// The result of neighborhood_statistics.
    struct neighborhood_statistics_result
    {
        /// Moves enumerated.
        std::size_t moves{};
        /// Valid moves that lead to a better cost.
        std::size_t improving{};
        /// Valid moves that lead to neither a better nor a worse cost.
        std::size_t sideways{};
        /// Valid moves that lead to a worse cost.
        std::size_t worsening{};
        /// Moves enumerated that are not valid.
        std::size_t invalid{};
    };

    /// The result of check_neighborhood_costs.
    struct neighborhood_cost_check_result
    {
        /// Moves enumerated.
        std::size_t moves{};
        /// Moves whose delta evaluation disagrees with the full evaluation.
        std::size_t mismatches{};
        /// Moves enumerated that are not valid, or lead to an invalid solution.
        std::size_t invalid{};
    };

    /// The result of check_move_independence.
    struct move_independence_result
    {
        /// Moves enumerated.
        std::size_t moves{};
        /// Valid moves that leave the solution unchanged.
        std::size_t null_moves{};
        /// Valid moves that lead to a solution an earlier move led to.
        std::size_t repeated_states{};
        /// Moves enumerated that are not valid.
        std::size_t invalid{};
    };

    /// The result of check_random_move_distribution.
    struct random_distribution_result
    {
        /// Valid moves enumerated.
        std::size_t neighborhood_size{};
        /// Random moves drawn.
        std::size_t samples{};
        /// Draws that found no move, or a move not among the valid enumerated
        /// ones.
        std::size_t out_of_neighborhood{};
        /// Valid enumerated moves never drawn.
        std::size_t unseen{};
        /// The fewest draws of a valid enumerated move.
        std::size_t min_frequency{};
        /// The most draws of a valid enumerated move.
        std::size_t max_frequency{};
    };

    /// One cost component on the current solution, for people.
    struct component_report
    {
        /// The component's name(), or `#<position>`, from 1.
        std::string name;
        /// The component's own value, without weights.
        std::string value;
        /// Its describe(solution) text; empty without it.
        std::string description;
    };

    /// A move of neighborhood_preview, with its cost.
    struct inspected_move
    {
        /// The move.
        move_type move;
        /// The cost of the solution the move leads to.
        cost_type cost;
    };

    /// The result of neighborhood_preview.
    struct neighborhood_preview_result
    {
        /// Moves enumerated.
        std::size_t moves{};
        /// Moves enumerated that are not valid.
        std::size_t invalid{};
        /// The first valid moves, with their costs.
        std::vector<inspected_move> entries;
    };
    /// Whether the Input can be read from a stream.
    static constexpr bool supports_input_loading = readable_input<input_type>;
    /// Whether a solution can be read from a stream.
    static constexpr bool supports_solution_loading =
        readable_solution<input_type, solution_type>;
    /// Whether a solution can be written to a stream.
    static constexpr bool supports_solution_saving =
        writable_solution<input_type, solution_type>;

    static_assert(
        supports_initial_solution || supports_random_solution
            || supports_solution_loading,
        "Session requires the SolutionManager to provide initial_solution() "
        "or random_solution(std::mt19937_64&) or solution stream loading "
        "to be available");

    /// A session without an Input yet: set_input or load_input provides it, as
    /// in an interactive frontend.
    ///
    /// The seed initializes the RNG of the session, which draws random
    /// solutions and seeds the generator of each run.
    explicit Session(App application, const std::uint64_t seed = 0)
        : app_{std::move(application)}, rng_{seed}
    {
    }

    /// A session on an Input, which it owns: the app bound to it, and the RNG
    /// seeded with seed.
    ///
    /// Another Input is another session.
    Session(App application, input_type input, const std::uint64_t seed)
        : Session{std::move(application), seed}
    {
        set_input(std::move(input));
    }

    /// A session on an Input it shares with its other owners, for example an
    /// adapter that keeps the Input to encode the results.
    Session(
        App application,
        std::shared_ptr<const input_type> input,
        const std::uint64_t seed)
        : Session{std::move(application), seed}
    {
        set_input(std::move(input));
    }

    /// Seeds the RNG of the session.
    void set_seed(const std::uint64_t seed)
    {
        rng_.seed(seed);
    }

    /// The RNG of the session.
    [[nodiscard]]
    rng_type& rng() noexcept
    {
        return rng_;
    }

    /// The app.
    template<class Self>
    [[nodiscard]]
    auto& app(this Self&& self) noexcept
    {
        return self.app_;
    }

    /// Whether the session has an Input.
    [[nodiscard]]
    bool has_input() const noexcept
    {
        return static_cast<bool>(input_);
    }

    /// Sets the Input, which the session owns, and binds the app to it; the
    /// solution and the move are dropped.
    void set_input(input_type input)
    {
        set_input(std::make_shared<const input_type>(std::move(input)));
    }

    /// Sets an Input shared with other owners and binds the app to it; the
    /// solution and the move are dropped.
    ///
    /// Throws std::invalid_argument when new_input is null.
    void set_input(std::shared_ptr<const input_type> new_input)
    {
        if (!new_input)
            throw std::invalid_argument{"Session::set_input: no Input"};
        auto new_bound =
            std::unique_ptr<bound_app_type>{new bound_app_type(app_.bind(*new_input))};

        clear_solution_state();
        last_run_effort_.reset();
        bound_.reset();
        input_ = std::move(new_input);
        bound_ = std::move(new_bound);
    }

    /// Reads the Input from a stream with the problem's read hook, as
    /// set_input.
    void load_input(std::istream& in)
        requires supports_input_loading
    {
        set_input(easylocal::read_input<input_type>(in));
    }

    /// Reads the Input from a file, as set_input; throws std::runtime_error
    /// when the file cannot be opened.
    ///
    /// Unlike easylocal::load_input, errors do not name the file, which an
    /// interactive frontend shows on its own.
    void load_input(const std::filesystem::path& path)
        requires supports_input_loading
    {
        std::ifstream in{path};
        if (!in)
            throw std::runtime_error{"failed to open Input file: " + path.string()};
        load_input(in);
    }

    /// The Input; the session must have one.
    [[nodiscard]]
    const input_type& input() const noexcept
    {
        assert(input_);
        return *input_;
    }

    /// The shared Input; null without one.
    [[nodiscard]]
    std::shared_ptr<const input_type> input_handle() const noexcept
    {
        return input_;
    }

    /// The app bound to the Input; the session must have one.
    template<class Self>
    [[nodiscard]]
    auto& bound_app(this Self&& self) noexcept
    {
        assert(self.bound_);
        // const when the session is: a unique_ptr does not carry it over.
        return std::forward_like<Self&>(*self.bound_);
    }

    /// Whether the session has a current solution.
    [[nodiscard]]
    bool has_solution() const noexcept
    {
        return static_cast<bool>(solution_);
    }

    /// Makes the SolutionManager's initial_solution() the current solution.
    void use_initial_solution()
        requires supports_initial_solution
    {
        assert(bound_);
        solution_ = std::make_unique<solution_type>(
            bound_->solution_manager().initial_solution());
        clear_move_state();
    }

    /// Makes a random_solution(rng) of the SolutionManager the current
    /// solution.
    void use_random_solution(rng_type& rng)
        requires supports_random_solution
    {
        assert(bound_);
        solution_ = std::make_unique<solution_type>(
            bound_->solution_manager().random_solution(rng));
        clear_move_state();
    }

    /// Makes solution the current solution.
    void set_solution(solution_type solution)
    {
        assert(bound_);
        solution_ = std::make_unique<solution_type>(std::move(solution));
        clear_move_state();
    }

    /// Reads the current solution from a stream with the problem's read hook.
    void load_solution(std::istream& in)
        requires supports_solution_loading
    {
        assert(input_);
        set_solution(easylocal::read_solution<solution_type>(*input_, in));
    }

    /// Reads the current solution from a file; throws std::runtime_error when
    /// the file cannot be opened.
    void load_solution(const std::filesystem::path& path)
        requires supports_solution_loading
    {
        std::ifstream in{path};
        if (!in)
            throw std::runtime_error{"failed to open Solution file: " + path.string()};
        load_solution(in);
    }

    /// Writes the current solution to a stream with the problem's write hook.
    void save_solution(std::ostream& out) const
        requires supports_solution_saving
    {
        assert(input_);
        assert(solution_);
        easylocal::write_solution(*input_, *solution_, out);
    }

    /// Writes the current solution to a file.
    void save_solution(const std::filesystem::path& path) const
        requires supports_solution_saving
    {
        assert(input_);
        assert(solution_);
        easylocal::save_solution(*input_, *solution_, path);
    }

    /// The current solution; the session must have one.
    [[nodiscard]]
    const solution_type& solution() const noexcept
    {
        assert(solution_);
        return *solution_;
    }

    /// Whether the current solution is valid for the SolutionManager.
    [[nodiscard]]
    bool is_valid() const
    {
        assert(bound_);
        assert(solution_);
        return static_cast<bool>(bound_->solution_manager().is_valid(*solution_));
    }

    /// The cost of the current solution, which must be valid.
    [[nodiscard]]
    cost_type evaluate() const
    {
        assert(bound_);
        assert(solution_);
        assert(is_valid());
        return bound_->solution_manager().evaluate(*solution_);
    }

    /// Each cost component of the current solution, in the order of the
    /// recipe: its name, its value and, when the component has
    /// describe(solution), the text that explains it.
    [[nodiscard]]
    std::vector<component_report> cost_report() const
        requires requires { typename solution_manager_type::component_types; }
    {
        assert(bound_);
        assert(solution_);
        using component_types = typename solution_manager_type::component_types;
        std::vector<component_report> report;
        report.reserve(std::tuple_size_v<component_types>);
        [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            (report.push_back(component_entry<Index>()), ...);
        }(std::make_index_sequence<std::tuple_size_v<component_types>>{});
        return report;
    }

    /// A cost written as text, such as a target: by the problem's
    /// read_cost(input, text) when it has one, else as cost::from_text reads
    /// it.
    ///
    /// Throws std::invalid_argument when the text is not a cost.
    [[nodiscard]]
    cost_type read_cost(const std::string_view text) const
        requires readable_cost<input_type, cost_type>
    {
        assert(input_);
        return easylocal::read_cost<cost_type>(*input_, text);
    }

    /// The contract checks of the app from the current solution:
    /// check(app, input, solution).
    [[nodiscard]]
    app_check_report check() const
    {
        assert(input_);
        assert(solution_);
        return easylocal::check(app_, *input_, *solution_);
    }

    /// The parameters of the app, for reading: cost.*, neighborhood.* and
    /// `runners.<name>.*` (app.configuration()).
    [[nodiscard]]
    config::parameter_set configuration() const&
        requires requires(const App& application) { application.configuration(); }
    {
        return app_.configuration();
    }

    /// Deleted: the set of a temporary would refer to it after it is gone.
    /// Configure the object that will run, after its last copy.
    config::parameter_set configuration() const&& = delete;

    /// Changes the parameters of the app, with the paths of configuration():
    /// all of them, validated, or none.
    ///
    /// The session's services are rebuilt with the new values, so the costs it
    /// reports follow them; the current solution stays and the selected move is
    /// cleared. The runners read their parameters at each run. When the
    /// rebuilding throws, the app gets its previous values back first.
    config::override_result configure(
        const std::span<const config::text_override> overrides)
        requires requires(App& application) { application.configuration(); }
    {
        const auto previous = bound_
            ? app_.configuration().parameters()
            : std::vector<config::parameter_info>{};
        auto result = app_.configuration().apply(overrides);
        if (result && bound_)
        {
            try
            {
                bound_ = std::unique_ptr<bound_app_type>{
                    new bound_app_type(app_.bind(*input_))};
            }
            catch (...)
            {
                std::vector<config::text_override> restore;
                for (const auto& parameter : previous)
                    if (!parameter.read_only)
                        restore.push_back({parameter.path, parameter.value});
                static_cast<void>(app_.configuration().apply(restore));
                throw;
            }
            clear_move_state();
        }
        return result;
    }

    /// The names of the registered runners and pipelines, in the order of
    /// registration.
    [[nodiscard]]
    std::vector<std::string_view> runner_names() const
    {
        return app_.runner_names();
    }

    /// Runs the runner or pipeline registered under name from the current
    /// solution, which it replaces with the result; false when nothing has
    /// that name.
    ///
    /// The run gets a generator of its own, seeded with one draw of the
    /// session's RNG (`rng_type{rng()()}`), so successive runs differ and a
    /// seed reproduces them in order. The options are run options, such as
    /// with(control, tracer), stop_at,
    /// timeout and max_evaluations. Like every app run, it uses freshly bound
    /// services and the current runner parameters, not this session's bound
    /// app. Throws `std::invalid_argument`, and changes nothing, when the
    /// current solution is not valid for the Input (set_solution and
    /// load_solution accept one, to inspect it).
    template<class... Options>
    [[nodiscard]]
    bool run(const std::string_view name, Options&&... options)
    {
        assert(bound_);
        assert(solution_);
        if (!is_valid())
        {
            throw std::invalid_argument{
                "run: the current solution is not valid for the Input"};
        }
        // The effort is the last run's: none until this one completes.
        last_run_effort_.reset();
        const auto names = app_.runner_names();
        if (std::ranges::find(names, name) == names.end())
            return false;

        // Each run draws its own generator from the session's, as the TextUI
        // does for its background runs: the same seed and the same commands
        // give the same runs in every frontend.
        rng_type run_rng{rng_()};
        auto result = app_.run(
            name,
            *input_,
            *solution_,
            run_rng,
            std::forward<Options>(options)...);
        if (!result)
            return false;

        solution_ = std::make_unique<solution_type>(std::move(result->solution));
        last_run_effort_ = result->effort;
        clear_move_state();
        return true;
    }

    /// The effort of the last run (evaluations, iterations, termination), when
    /// its algorithm reports it; empty before the first run, after a run that
    /// did not complete (an unknown name, an exception) and after a new Input.
    [[nodiscard]]
    const std::optional<run_effort>& last_run_effort() const noexcept
    {
        return last_run_effort_;
    }

    /// Whether a move is selected.
    [[nodiscard]]
    bool has_move() const noexcept
    {
        return move_.has_value();
    }

    /// The selected move; one must be selected.
    [[nodiscard]]
    const move_type& move() const noexcept
    {
        assert(move_);
        return *move_;
    }

    /// Selects move.
    void set_move(move_type move)
    {
        assert(solution_);
        move_.emplace(std::move(move));
        deterministic_move_index_.reset();
    }

    /// Selects the first enumerated move; false when the neighborhood is empty.
    [[nodiscard]]
    bool use_first_move()
        requires supports_deterministic_moves
    {
        assert(bound_);
        assert(solution_);
        return select_deterministic_move(0);
    }

    /// Selects the move enumerated after the selected one; false when there is
    /// none, or the selected move was not enumerated.
    [[nodiscard]]
    bool use_next_move()
        requires supports_deterministic_moves
    {
        assert(bound_);
        assert(solution_);

        if (!deterministic_move_index_)
            return false;

        return select_deterministic_move(*deterministic_move_index_ + 1);
    }

    /// Selects the first valid move that improves the current cost; false, with
    /// no move selected, when there is none.
    [[nodiscard]]
    bool use_first_improving_move()
        requires supports_improvement_selection
    {
        assert(bound_);
        assert(solution_);

        const auto evaluation = this->evaluation();
        const auto current = evaluation.evaluate(*solution_);
        std::size_t index = 0;
        for (auto&& candidate : easylocal::moves(bound_->neighborhood(), *solution_))
        {
            move_.emplace(candidate);
            deterministic_move_index_ = index;
            if (move_is_valid()
                && cost::better(
                    bound_->solution_manager(),
                    evaluation.evaluate_move(*solution_, current, *move_).cost(),
                    current.cost()))
            {
                return true;
            }
            ++index;
        }
        clear_move_state();
        return false;
    }

    /// Selects the valid move of the best cost, the first among equals; false,
    /// with no move selected, when there is none.
    [[nodiscard]]
    bool use_best_move()
        requires supports_improvement_selection
    {
        assert(bound_);
        assert(solution_);

        const auto evaluation = this->evaluation();
        const auto current = evaluation.evaluate(*solution_);
        std::optional<move_type> best_move;
        std::optional<cost_type> best_cost;
        std::optional<std::size_t> best_index;
        std::size_t index = 0;
        for (auto&& candidate : easylocal::moves(bound_->neighborhood(), *solution_))
        {
            move_.emplace(candidate);
            deterministic_move_index_ = index;
            if (move_is_valid())
            {
                auto candidate_cost =
                    evaluation.evaluate_move(*solution_, current, *move_).cost();
                if (!best_cost
                    || cost::better(
                        bound_->solution_manager(),
                        candidate_cost,
                        *best_cost))
                {
                    best_move = candidate;
                    best_cost = std::move(candidate_cost);
                    best_index = index;
                }
            }
            ++index;
        }

        if (!best_move)
        {
            clear_move_state();
            return false;
        }
        move_ = std::move(best_move);
        deterministic_move_index_ = best_index;
        return true;
    }

    /// Selects a random move; false, with no move selected, when none is drawn.
    [[nodiscard]]
    bool use_random_move(rng_type& rng)
        requires supports_random_moves
    {
        assert(bound_);
        assert(solution_);

        auto selected = easylocal::random_move(bound_->neighborhood(), *solution_, rng);

        deterministic_move_index_.reset();
        if (!selected)
        {
            move_.reset();
            return false;
        }

        move_.emplace(std::move(*selected));
        return true;
    }

    /// Whether the selected move is valid on the current solution.
    [[nodiscard]]
    bool move_is_valid() const
    {
        assert(bound_);
        assert(solution_);
        assert(move_);
        return static_cast<bool>(bound_->neighborhood().is_valid(*solution_, *move_));
    }

    /// The cost of the solution the selected move leads to, by delta
    /// evaluation; the move must be valid.
    [[nodiscard]]
    cost_type evaluate_move() const
    {
        assert(bound_);
        assert(solution_);
        assert(move_);
        assert(move_is_valid());

        const auto evaluation = this->evaluation();
        return evaluation
            .evaluate_move(*solution_, evaluation.evaluate(*solution_), *move_)
            .cost();
    }

    /// The cost of the solution the selected move leads to, by applying the
    /// move to a copy and evaluating it fully; the move must be valid.
    [[nodiscard]]
    cost_type evaluate_move_fully() const
    {
        assert(bound_);
        assert(solution_);
        assert(move_);
        assert(move_is_valid());

        auto candidate = *solution_;
        bound_->neighborhood().make_move(candidate, *move_);
        assert(bound_->solution_manager().is_valid(candidate));
        return bound_->solution_manager().evaluate(candidate);
    }

    /// Whether evaluate_move and evaluate_move_fully agree, by the cost's
    /// equivalent().
    [[nodiscard]]
    bool move_evaluation_matches_full() const
        requires cost::has_equivalent<solution_manager_type>
    {
        const auto incremental = evaluate_move();
        const auto full = evaluate_move_fully();
        return cost::equivalent(bound_->solution_manager(), incremental, full);
    }

    /// The moves enumerated from the current solution, counted, the invalid
    /// ones apart, and the first max_entries valid ones with their costs.
    [[nodiscard]]
    neighborhood_preview_result neighborhood_preview(
        const std::size_t max_entries = 8) const
        requires supports_deterministic_moves
    {
        assert(bound_);
        assert(solution_);

        neighborhood_preview_result result;
        const auto& neighborhood = bound_->neighborhood();
        const auto evaluation = this->evaluation();
        const auto current = evaluation.evaluate(*solution_);

        for (auto&& raw_move : easylocal::moves(neighborhood, *solution_))
        {
            move_type candidate{raw_move};
            ++result.moves;
            if (!static_cast<bool>(neighborhood.is_valid(*solution_, candidate)))
            {
                ++result.invalid;
                continue;
            }
            if (result.entries.size() == max_entries)
                continue;
            auto evaluated = evaluation.evaluate_move(*solution_, current, candidate);
            result.entries.push_back(
                inspected_move{
                    .move = std::move(candidate),
                    .cost = evaluated.cost(),
                });
        }
        return result;
    }

    /// Counts the enumerated moves from the current solution that improve, keep
    /// or worsen its cost, and the invalid ones.
    [[nodiscard]]
    neighborhood_statistics_result neighborhood_statistics() const
        requires supports_improvement_selection
    {
        assert(bound_);
        assert(solution_);

        neighborhood_statistics_result result;
        const auto& solution_manager = bound_->solution_manager();
        const auto& neighborhood = bound_->neighborhood();
        const auto evaluation = this->evaluation();
        const auto current = evaluation.evaluate(*solution_);

        for (auto&& raw_move : easylocal::moves(neighborhood, *solution_))
        {
            move_type candidate{raw_move};
            ++result.moves;
            if (!static_cast<bool>(neighborhood.is_valid(*solution_, candidate)))
            {
                ++result.invalid;
                continue;
            }
            const auto candidate_cost =
                evaluation.evaluate_move(*solution_, current, candidate).cost();
            if (cost::better(solution_manager, candidate_cost, current.cost()))
                ++result.improving;
            else if (cost::better(solution_manager, current.cost(), candidate_cost))
                ++result.worsening;
            else
                ++result.sideways;
        }
        return result;
    }

    /// Compares the delta evaluation of each enumerated move with the full
    /// evaluation of the solution it leads to.
    [[nodiscard]]
    neighborhood_cost_check_result check_neighborhood_costs() const
        requires supports_cost_consistency_check
    {
        assert(bound_);
        assert(solution_);

        neighborhood_cost_check_result result;
        const auto& solution_manager = bound_->solution_manager();
        const auto& neighborhood = bound_->neighborhood();
        const auto evaluation = this->evaluation();
        const auto current = evaluation.evaluate(*solution_);

        for (auto&& raw_move : easylocal::moves(neighborhood, *solution_))
        {
            move_type move{raw_move};
            ++result.moves;
            if (!static_cast<bool>(neighborhood.is_valid(*solution_, move)))
            {
                ++result.invalid;
                continue;
            }

            const auto incremental =
                evaluation.evaluate_move(*solution_, current, move).cost();
            auto candidate = *solution_;
            neighborhood.make_move(candidate, move);
            if (!static_cast<bool>(solution_manager.is_valid(candidate)))
            {
                ++result.invalid;
                continue;
            }
            const auto full = solution_manager.evaluate(candidate);
            if (!cost::equivalent(solution_manager, incremental, full))
                ++result.mismatches;
        }
        return result;
    }

    /// Counts the enumerated moves that leave the current solution unchanged,
    /// and those that lead to a solution an earlier move led to.
    [[nodiscard]]
    move_independence_result check_move_independence() const
        requires supports_move_independence_check
    {
        assert(bound_);
        assert(solution_);

        move_independence_result result;
        const auto& neighborhood = bound_->neighborhood();
        std::vector<solution_type> reached;

        for (auto&& raw_move : easylocal::moves(neighborhood, *solution_))
        {
            move_type move{raw_move};
            ++result.moves;
            if (!static_cast<bool>(neighborhood.is_valid(*solution_, move)))
            {
                ++result.invalid;
                continue;
            }

            auto candidate = *solution_;
            neighborhood.make_move(candidate, move);
            if (candidate == *solution_)
            {
                ++result.null_moves;
                continue;
            }

            bool repeated = false;
            for (const auto& previous : reached)
            {
                if (candidate == previous)
                {
                    repeated = true;
                    break;
                }
            }
            if (repeated)
                ++result.repeated_states;
            else
                reached.push_back(std::move(candidate));
        }
        return result;
    }

    /// Draws rounds_per_move random moves per valid enumerated move and counts
    /// how often each is drawn, and the draws outside the enumerated moves.
    [[nodiscard]]
    random_distribution_result check_random_move_distribution(
        rng_type& rng,
        const std::size_t rounds_per_move = 20) const
        requires supports_random_distribution_check
    {
        assert(bound_);
        assert(solution_);

        random_distribution_result result;
        const auto& neighborhood = bound_->neighborhood();
        std::vector<move_type> moves_list;
        for (auto&& raw_move : easylocal::moves(neighborhood, *solution_))
        {
            move_type move{raw_move};
            if (static_cast<bool>(neighborhood.is_valid(*solution_, move)))
                moves_list.push_back(std::move(move));
        }

        result.neighborhood_size = moves_list.size();
        if (moves_list.empty() || rounds_per_move == 0)
            return result;

        std::vector<std::size_t> frequencies(moves_list.size());
        result.samples = moves_list.size() * rounds_per_move;
        for (std::size_t sample = 0; sample < result.samples; ++sample)
        {
            auto selected = easylocal::random_move(neighborhood, *solution_, rng);
            if (!selected)
            {
                ++result.out_of_neighborhood;
                continue;
            }
            bool matched = false;
            for (std::size_t index = 0; index < moves_list.size(); ++index)
            {
                if (*selected == moves_list[index])
                {
                    ++frequencies[index];
                    matched = true;
                    break;
                }
            }
            if (!matched)
                ++result.out_of_neighborhood;
        }

        result.min_frequency = frequencies.front();
        result.max_frequency = frequencies.front();
        for (const auto frequency : frequencies)
        {
            if (frequency == 0)
                ++result.unseen;
            result.min_frequency = std::min(result.min_frequency, frequency);
            result.max_frequency = std::max(result.max_frequency, frequency);
        }
        return result;
    }

    /// Applies the selected move, which must be valid, to the current solution,
    /// and drops it.
    ///
    /// A faulty make_move may leave a solution that is not valid: check
    /// is_valid() before evaluating it or running from it.
    void apply_move()
    {
        assert(bound_);
        assert(solution_);
        assert(move_);
        assert(move_is_valid());

        bound_->neighborhood().make_move(*solution_, *move_);
        clear_move_state();
    }

private:
    template<std::size_t Index>
    [[nodiscard]]
    component_report component_entry() const
    {
        using component_type =
            std::tuple_element_t<Index, typename solution_manager_type::component_types>;
        const auto& solution_manager = bound_->solution_manager();
        const auto& component = solution_manager.template component<component_type>();

        component_report entry;
        if constexpr (detail::named_component<component_type>)
            entry.name = std::string{component.name()};
        else
            entry.name = "#" + std::to_string(Index + 1);
        entry.value = detail::report_text(
            solution_manager.template evaluate_component<Index>(*solution_));
        if constexpr (detail::describing_component<component_type, solution_type>)
            entry.description = component.describe(*solution_);
        return entry;
    }

    // The incremental evaluation of the bound app: the current solution is
    // evaluated once per scan, then each move against it.
    [[nodiscard]]
    detail::evaluation_facility<solution_manager_type, neighborhood_type> evaluation()
        const
    {
        return {bound_->solution_manager(), bound_->neighborhood()};
    }

    void clear_move_state() noexcept
    {
        move_.reset();
        deterministic_move_index_.reset();
    }

    void clear_solution_state() noexcept
    {
        clear_move_state();
        solution_.reset();
    }

    [[nodiscard]]
    bool select_deterministic_move(const std::size_t target)
        requires supports_deterministic_moves
    {
        std::size_t index = 0;
        for (auto&& candidate : easylocal::moves(bound_->neighborhood(), *solution_))
        {
            if (index == target)
            {
                move_.emplace(candidate);
                deterministic_move_index_ = target;
                return true;
            }
            ++index;
        }

        if (target == 0)
            clear_move_state();
        return false;
    }

    App app_;
    std::shared_ptr<const input_type> input_;
    std::unique_ptr<bound_app_type> bound_;
    std::unique_ptr<solution_type> solution_;
    std::optional<run_effort> last_run_effort_;
    std::optional<move_type> move_;
    std::optional<std::size_t> deterministic_move_index_;
    rng_type rng_;
};

} // namespace easylocal
