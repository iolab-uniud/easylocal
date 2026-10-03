#pragma once

// Session: the state of an interactive session on an app (Input, current
// solution, selected move, RNG) and the commands that change it. It replaces
// EasyLocal 3's Tester; the TextUI and the REST adapter are views on it.

#include <easylocal/app/check.hpp>
#include <easylocal/app/io.hpp>
#include <easylocal/app/run_parameters.hpp>
#include <easylocal/config/parameter_set.hpp>
#include <easylocal/cost/semantics.hpp>
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

} // namespace detail

// The state of an interactive session on an app, and the commands that change
// it: an owned Input, the app bound to it, a current solution, a selected move
// and an RNG. It has no user interface of its own: interactive frontends, such
// as the TextUI, are views on it.
template<class App>
    requires std::move_constructible<App> && requires { typename App::input_type; }
class Session
{
public:
    using app_type = App;
    using input_type = typename App::input_type;
    using bound_app_type =
        decltype(std::declval<const App&>().bind(std::declval<const input_type&>()));
    using solution_manager_type = typename bound_app_type::solution_manager_type;
    using neighborhood_type = typename bound_app_type::neighborhood_explorer_type;
    using solution_type = typename solution_manager_type::solution_type;
    using cost_type = typename solution_manager_type::cost_type;
    using move_type = typename neighborhood_type::move_type;
    using rng_type = std::mt19937_64;

    static constexpr bool supports_initial_solution =
        has_initial_solution<solution_manager_type>;
    static constexpr bool supports_random_solution =
        has_random_solution<solution_manager_type, rng_type>;
    static constexpr bool supports_deterministic_moves =
        deterministic_neighborhood_for<neighborhood_type, solution_type>;
    static constexpr bool supports_random_moves =
        random_neighborhood_for<neighborhood_type, solution_type, rng_type>;
    static constexpr bool supports_improvement_selection =
        supports_deterministic_moves && cost::has_better<solution_manager_type>;
    static constexpr bool supports_cost_consistency_check =
        supports_deterministic_moves && cost::has_equivalent<solution_manager_type>;
    static constexpr bool supports_move_independence_check =
        supports_deterministic_moves && std::equality_comparable<solution_type>;
    static constexpr bool supports_random_distribution_check =
        supports_deterministic_moves && supports_random_moves
        && detail::comparable_moves_v<move_type>;

    struct neighborhood_statistics_result
    {
        std::size_t moves{};
        std::size_t improving{};
        std::size_t sideways{};
        std::size_t worsening{};
        std::size_t invalid{};
    };

    struct neighborhood_cost_check_result
    {
        std::size_t moves{};
        std::size_t mismatches{};
        std::size_t invalid{};
    };

    struct move_independence_result
    {
        std::size_t moves{};
        std::size_t null_moves{};
        std::size_t repeated_states{};
        std::size_t invalid{};
    };

    struct random_distribution_result
    {
        std::size_t neighborhood_size{};
        std::size_t samples{};
        std::size_t out_of_neighborhood{};
        std::size_t unseen{};
        std::size_t min_frequency{};
        std::size_t max_frequency{};
    };

    struct inspected_move
    {
        move_type move;
        cost_type cost;
    };

    struct neighborhood_preview_result
    {
        std::size_t moves{};
        std::vector<inspected_move> entries;
    };
    static constexpr bool supports_input_loading = readable_input<input_type>;
    static constexpr bool supports_solution_loading =
        readable_solution<input_type, solution_type>;
    static constexpr bool supports_solution_saving =
        writable_solution<input_type, solution_type>;

    static_assert(
        supports_initial_solution || supports_random_solution
            || supports_solution_loading,
        "Session requires the SolutionManager to provide initial_solution() "
        "or random_solution(std::mt19937_64&) or solution stream loading "
        "to be available");

    // A session without an Input yet: set_input or load_input provides it, as
    // in an interactive frontend. The seed initializes the RNG the session
    // gives to stochastic runners.
    explicit Session(App application, const std::uint64_t seed = 0)
        : app_{std::move(application)}, rng_{seed}
    {
    }

    // A session on an Input, which it owns: the app bound to it, and the RNG
    // seeded with seed. Another Input is another session.
    Session(App application, input_type input, const std::uint64_t seed)
        : Session{std::move(application), seed}
    {
        set_input(std::move(input));
    }

    // A session on an Input it shares with its other owners, for example an
    // adapter that keeps the Input to encode the results.
    Session(
        App application,
        std::shared_ptr<const input_type> input,
        const std::uint64_t seed)
        : Session{std::move(application), seed}
    {
        set_input(std::move(input));
    }

    void set_seed(const std::uint64_t seed)
    {
        rng_.seed(seed);
    }

    [[nodiscard]]
    rng_type& rng() noexcept
    {
        return rng_;
    }

    [[nodiscard]]
    App& app() noexcept
    {
        return app_;
    }

    [[nodiscard]]
    const App& app() const noexcept
    {
        return app_;
    }

    [[nodiscard]]
    bool has_input() const noexcept
    {
        return static_cast<bool>(input_);
    }

    void set_input(input_type input)
    {
        set_input(std::make_shared<const input_type>(std::move(input)));
    }

    void set_input(std::shared_ptr<const input_type> new_input)
    {
        if (!new_input)
            throw std::invalid_argument{"Session::set_input: no Input"};
        auto new_bound =
            std::unique_ptr<bound_app_type>{new bound_app_type(app_.bind(*new_input))};

        clear_solution_state();
        bound_.reset();
        input_ = std::move(new_input);
        bound_ = std::move(new_bound);
    }

    void load_input(std::istream& in)
        requires supports_input_loading
    {
        set_input(easylocal::read_input<input_type>(in));
    }

    // Unlike easylocal::load_input, errors do not name the file, which an
    // interactive frontend shows on its own.
    void load_input(const std::filesystem::path& path)
        requires supports_input_loading
    {
        std::ifstream in{path};
        if (!in)
            throw std::runtime_error{"failed to open Input file: " + path.string()};
        load_input(in);
    }

    [[nodiscard]]
    const input_type& input() const noexcept
    {
        assert(input_);
        return *input_;
    }

    [[nodiscard]]
    std::shared_ptr<const input_type> input_handle() const noexcept
    {
        return input_;
    }

    [[nodiscard]]
    bound_app_type& bound_app() noexcept
    {
        assert(bound_);
        return *bound_;
    }

    [[nodiscard]]
    const bound_app_type& bound_app() const noexcept
    {
        assert(bound_);
        return *bound_;
    }

    [[nodiscard]]
    bool has_solution() const noexcept
    {
        return static_cast<bool>(solution_);
    }

    void use_initial_solution()
        requires supports_initial_solution
    {
        assert(bound_);
        solution_ = std::make_unique<solution_type>(
            bound_->solution_manager().initial_solution());
        clear_move_state();
    }

    void use_random_solution(rng_type& rng)
        requires supports_random_solution
    {
        assert(bound_);
        solution_ = std::make_unique<solution_type>(
            bound_->solution_manager().random_solution(rng));
        clear_move_state();
    }

    void set_solution(solution_type solution)
    {
        assert(bound_);
        solution_ = std::make_unique<solution_type>(std::move(solution));
        clear_move_state();
    }

    void load_solution(std::istream& in)
        requires supports_solution_loading
    {
        assert(input_);
        set_solution(easylocal::read_solution<solution_type>(*input_, in));
    }

    void load_solution(const std::filesystem::path& path)
        requires supports_solution_loading
    {
        std::ifstream in{path};
        if (!in)
            throw std::runtime_error{"failed to open Solution file: " + path.string()};
        load_solution(in);
    }

    void save_solution(std::ostream& out) const
        requires supports_solution_saving
    {
        assert(input_);
        assert(solution_);
        easylocal::write_solution(*input_, *solution_, out);
    }

    void save_solution(const std::filesystem::path& path) const
        requires supports_solution_saving
    {
        assert(input_);
        assert(solution_);
        easylocal::save_solution(*input_, *solution_, path);
    }

    [[nodiscard]]
    const solution_type& solution() const noexcept
    {
        assert(solution_);
        return *solution_;
    }

    [[nodiscard]]
    bool is_valid() const
    {
        assert(bound_);
        assert(solution_);
        return static_cast<bool>(bound_->solution_manager().is_valid(*solution_));
    }

    [[nodiscard]]
    cost_type evaluate() const
    {
        assert(bound_);
        assert(solution_);
        assert(is_valid());
        return bound_->solution_manager().evaluate(*solution_);
    }

    // A cost written as text, such as a target: by the problem's
    // read_cost(input, text) when it has one, else as cost::from_text reads it.
    // Throws std::invalid_argument when the text is not a cost.
    [[nodiscard]]
    cost_type read_cost(const std::string_view text) const
        requires readable_cost<input_type, cost_type>
    {
        assert(input_);
        return easylocal::read_cost<cost_type>(*input_, text);
    }

    [[nodiscard]]
    app_check_report check() const
    {
        assert(input_);
        assert(solution_);
        return easylocal::check(app_, *input_, *solution_);
    }

    // The parameters of the app, for reading: cost.*, neighborhood.* and
    // runners.<name>.* (app.configuration()).
    [[nodiscard]]
    config::parameter_set configuration() const
        requires requires(const App& application) { application.configuration(); }
    {
        return app_.configuration();
    }

    // Changes the parameters of the app, with the paths of configuration():
    // all of them, validated, or none. The session's services are rebuilt
    // with the new values, so the costs it reports follow them; the current
    // solution stays and the selected move is cleared. The runners read their
    // parameters at each run.
    config::override_result configure(
        const std::span<const config::text_override> overrides)
        requires requires(App& application) { application.configuration(); }
    {
        auto result = app_.configuration().apply(overrides);
        if (result && bound_)
        {
            bound_ =
                std::unique_ptr<bound_app_type>{new bound_app_type(app_.bind(*input_))};
            clear_move_state();
        }
        return result;
    }

    [[nodiscard]]
    std::vector<std::string_view> runner_names() const
    {
        std::vector<std::string_view> names;
        names.reserve(App::runner_count);
        app_.for_each_runner_registration(
            [&]<class Algorithm>(
                const std::string_view name,
                const typename Algorithm::parameters_type&) { names.push_back(name); });
        return names;
    }

    // Runs the runner registered under name from the current solution, which
    // it replaces with the runner's result; false when no runner has that name.
    // Options are with(control, tracer). Like every app run, it uses freshly
    // bound services and the current runner parameters, not this session's
    // bound app.
    template<class... Options>
    [[nodiscard]]
    bool run(const std::string_view name, Options&&... options)
    {
        assert(bound_);
        assert(solution_);
        assert(is_valid());

        auto result =
            app_.run(name, *input_, *solution_, rng_, std::forward<Options>(options)...);
        if (!result)
            return false;

        solution_ = std::make_unique<solution_type>(std::move(result->solution));
        clear_move_state();
        return true;
    }

    [[nodiscard]]
    bool has_move() const noexcept
    {
        return move_.has_value();
    }

    [[nodiscard]]
    const move_type& move() const noexcept
    {
        assert(move_);
        return *move_;
    }

    void set_move(move_type move)
    {
        assert(solution_);
        move_.emplace(std::move(move));
        deterministic_move_index_.reset();
    }

    [[nodiscard]]
    bool use_first_move()
        requires supports_deterministic_moves
    {
        assert(bound_);
        assert(solution_);
        return select_deterministic_move(0);
    }

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

    [[nodiscard]]
    bool use_first_improving_move()
        requires supports_improvement_selection
    {
        assert(bound_);
        assert(solution_);

        const auto current = evaluate();
        std::size_t index = 0;
        for (auto&& candidate : easylocal::moves(bound_->neighborhood(), *solution_))
        {
            move_.emplace(candidate);
            deterministic_move_index_ = index;
            if (move_is_valid()
                && cost::better(bound_->solution_manager(), evaluate_move(), current))
            {
                return true;
            }
            ++index;
        }
        clear_move_state();
        return false;
    }

    [[nodiscard]]
    bool use_best_move()
        requires supports_improvement_selection
    {
        assert(bound_);
        assert(solution_);

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
                auto candidate_cost = evaluate_move();
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

    [[nodiscard]]
    bool move_is_valid() const
    {
        assert(bound_);
        assert(solution_);
        assert(move_);
        return static_cast<bool>(bound_->neighborhood().is_valid(*solution_, *move_));
    }

    [[nodiscard]]
    cost_type evaluate_move() const
    {
        assert(bound_);
        assert(solution_);
        assert(move_);
        assert(move_is_valid());

        const auto& solution_manager = bound_->solution_manager();
        const auto& neighborhood = bound_->neighborhood();
        const detail::evaluation_facility<solution_manager_type, neighborhood_type>
            evaluation{
                solution_manager,
                neighborhood,
        };
        const auto current = evaluation.evaluate(*solution_);
        const auto candidate = evaluation.evaluate_move(*solution_, current, *move_);
        return candidate.cost();
    }

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

    [[nodiscard]]
    bool move_evaluation_matches_full() const
        requires cost::has_equivalent<solution_manager_type>
    {
        const auto incremental = evaluate_move();
        const auto full = evaluate_move_fully();
        return cost::equivalent(bound_->solution_manager(), incremental, full);
    }

    [[nodiscard]]
    neighborhood_preview_result neighborhood_preview(
        const std::size_t max_entries = 8) const
        requires supports_deterministic_moves
    {
        assert(bound_);
        assert(solution_);

        neighborhood_preview_result result;
        const auto& solution_manager = bound_->solution_manager();
        const auto& neighborhood = bound_->neighborhood();
        const detail::evaluation_facility<solution_manager_type, neighborhood_type>
            evaluation{solution_manager, neighborhood};
        const auto current = evaluation.evaluate(*solution_);

        for (auto&& raw_move : easylocal::moves(neighborhood, *solution_))
        {
            move_type candidate{raw_move};
            ++result.moves;
            if (result.entries.size() == max_entries
                || !static_cast<bool>(neighborhood.is_valid(*solution_, candidate)))
            {
                continue;
            }
            auto evaluated = evaluation.evaluate_move(*solution_, current, candidate);
            result.entries.push_back(
                inspected_move{
                    .move = std::move(candidate),
                    .cost = evaluated.cost(),
                });
        }
        return result;
    }

    [[nodiscard]]
    neighborhood_statistics_result neighborhood_statistics() const
        requires supports_improvement_selection
    {
        assert(bound_);
        assert(solution_);

        neighborhood_statistics_result result;
        const auto& solution_manager = bound_->solution_manager();
        const auto& neighborhood = bound_->neighborhood();
        const detail::evaluation_facility<solution_manager_type, neighborhood_type>
            evaluation{solution_manager, neighborhood};
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

    [[nodiscard]]
    neighborhood_cost_check_result check_neighborhood_costs() const
        requires supports_cost_consistency_check
    {
        assert(bound_);
        assert(solution_);

        neighborhood_cost_check_result result;
        const auto& solution_manager = bound_->solution_manager();
        const auto& neighborhood = bound_->neighborhood();
        const detail::evaluation_facility<solution_manager_type, neighborhood_type>
            evaluation{solution_manager, neighborhood};
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

    void apply_move()
    {
        assert(bound_);
        assert(solution_);
        assert(move_);
        assert(move_is_valid());

        bound_->neighborhood().make_move(*solution_, *move_);
        assert(bound_->solution_manager().is_valid(*solution_));
        clear_move_state();
    }

private:
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
    std::optional<move_type> move_;
    std::optional<std::size_t> deterministic_move_index_;
    rng_type rng_;
};

} // namespace easylocal
