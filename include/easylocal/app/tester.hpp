#pragma once

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
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <easylocal/app/check.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/cost/semantics.hpp>
#include <easylocal/helpers/detail/evaluation.hpp>
#include <easylocal/helpers/solution_manager.hpp>

namespace easylocal
{

namespace detail::tester_io
{

namespace adl
{

void read_input() = delete;
void read_solution() = delete;
void write_solution() = delete;

template<class Input>
concept has_read_input =
    requires(std::istream& in) {
        {
            read_input(std::type_identity<Input>{}, in)
        } -> std::convertible_to<Input>;
    };

template<class Input>
    requires has_read_input<Input>
[[nodiscard]]
Input call_read_input(std::istream& in)
{
    return read_input(std::type_identity<Input>{}, in);
}

template<class Input, class Solution>
concept has_read_solution =
    requires(const Input& input, std::istream& in) {
        {
            read_solution(input, in)
        } -> std::convertible_to<Solution>;
    };

template<class Input, class Solution>
    requires has_read_solution<Input, Solution>
[[nodiscard]]
Solution call_read_solution(const Input& input, std::istream& in)
{
    return read_solution(input, in);
}

template<class Input, class Solution>
concept has_write_solution =
    requires(
        const Input& input,
        const Solution& solution,
        std::ostream& out) {
        write_solution(input, solution, out);
    };

template<class Input, class Solution>
    requires has_write_solution<Input, Solution>
void call_write_solution(
    const Input& input,
    const Solution& solution,
    std::ostream& out)
{
    write_solution(input, solution, out);
}

} // namespace adl

template<class Input>
concept has_static_input_read =
    requires(std::istream& in) {
        {
            Input::read(in)
        } -> std::convertible_to<Input>;
    };

template<class Input>
concept has_input_stream_extraction =
    std::default_initializable<Input> &&
    requires(std::istream& in, Input& input) {
        in >> input;
    };

template<class Input>
concept readable_input =
    has_static_input_read<Input> ||
    adl::has_read_input<Input> ||
    has_input_stream_extraction<Input>;

template<class Input>
    requires readable_input<Input>
[[nodiscard]]
Input read_input(std::istream& in)
{
    if constexpr (has_static_input_read<Input>)
    {
        return Input::read(in);
    }
    else if constexpr (adl::has_read_input<Input>)
    {
        return adl::call_read_input<Input>(in);
    }
    else
    {
        Input input{};
        in >> input;
        return input;
    }
}

template<class Input, class Solution>
concept has_static_solution_read =
    requires(const Input& input, std::istream& in) {
        {
            Solution::read(input, in)
        } -> std::convertible_to<Solution>;
    };

template<class Input, class Solution>
concept has_solution_stream_extraction =
    std::constructible_from<Solution, const Input&> &&
    requires(std::istream& in, Solution& solution) {
        in >> solution;
    };

template<class Input, class Solution>
concept readable_solution =
    has_static_solution_read<Input, Solution> ||
    adl::has_read_solution<Input, Solution> ||
    has_solution_stream_extraction<Input, Solution>;

template<class Solution, class Input>
    requires readable_solution<Input, Solution>
[[nodiscard]]
Solution read_solution(const Input& input, std::istream& in)
{
    if constexpr (has_static_solution_read<Input, Solution>)
    {
        return Solution::read(input, in);
    }
    else if constexpr (adl::has_read_solution<Input, Solution>)
    {
        return adl::call_read_solution<Input, Solution>(input, in);
    }
    else
    {
        Solution solution{input};
        in >> solution;
        return solution;
    }
}

template<class Input, class Solution>
concept has_member_solution_write =
    requires(
        const Input& input,
        const Solution& solution,
        std::ostream& out) {
        solution.write(input, out);
    };

template<class Solution>
concept has_solution_stream_insertion =
    requires(std::ostream& out, const Solution& solution) {
        out << solution;
    };

template<class Input, class Solution>
concept writable_solution =
    has_member_solution_write<Input, Solution> ||
    adl::has_write_solution<Input, Solution> ||
    has_solution_stream_insertion<Solution>;

template<class Input, class Solution>
    requires writable_solution<Input, Solution>
void write_solution(
    const Input& input,
    const Solution& solution,
    std::ostream& out)
{
    if constexpr (has_member_solution_write<Input, Solution>)
    {
        solution.write(input, out);
    }
    else if constexpr (adl::has_write_solution<Input, Solution>)
    {
        adl::call_write_solution(input, solution, out);
    }
    else
    {
        out << solution;
    }
}

inline void require_read_success(const std::istream& in, std::string_view what)
{
    if (in.fail())
    {
        throw std::runtime_error{"failed to read " + std::string{what}};
    }
}

inline void require_write_success(const std::ostream& out, std::string_view what)
{
    if (out.fail())
    {
        throw std::runtime_error{"failed to write " + std::string{what}};
    }
}

} // namespace detail::tester_io

template<class App>
    requires std::move_constructible<App> &&
             requires { typename App::input_type; }
class Tester
{
public:
    using app_type = App;
    using input_type = typename App::input_type;
    using runtime_type = decltype(
        std::declval<const App&>().for_input(
            std::declval<const input_type&>()));
    using solution_manager_type = typename runtime_type::solution_manager_type;
    using neighborhood_type = typename runtime_type::neighborhood_explorer_type;
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
        supports_deterministic_moves && supports_random_moves &&
        std::equality_comparable<move_type>;

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
    static constexpr bool supports_input_loading =
        detail::tester_io::readable_input<input_type>;
    static constexpr bool supports_solution_loading =
        detail::tester_io::readable_solution<input_type, solution_type>;
    static constexpr bool supports_solution_saving =
        detail::tester_io::writable_solution<input_type, solution_type>;

    static_assert(
        supports_initial_solution ||
            supports_random_solution ||
            supports_solution_loading,
        "Tester requires the SolutionManager to provide initial_solution() "
        "or random_solution(std::mt19937_64&) or solution stream loading "
        "to be available");

    // The seed initializes the RNG the Tester gives to stochastic runners.
    explicit Tester(App application, const std::uint64_t seed = 0)
        : app_{std::move(application)},
          rng_{seed}
    {
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
        auto new_input = std::make_shared<const input_type>(std::move(input));
        auto new_runtime = std::unique_ptr<runtime_type>{
            new runtime_type(app_.for_input(*new_input))};

        clear_solution_state();
        runtime_.reset();
        input_ = std::move(new_input);
        runtime_ = std::move(new_runtime);
    }

    void load_input(std::istream& in)
        requires supports_input_loading
    {
        auto input = detail::tester_io::read_input<input_type>(in);
        detail::tester_io::require_read_success(in, "Input");
        set_input(std::move(input));
    }

    void load_input(const std::filesystem::path& path)
        requires supports_input_loading
    {
        std::ifstream in{path};
        if (!in)
        {
            throw std::runtime_error{
                "failed to open Input file: " + path.string()};
        }
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
    runtime_type& runtime() noexcept
    {
        assert(runtime_);
        return *runtime_;
    }

    [[nodiscard]]
    const runtime_type& runtime() const noexcept
    {
        assert(runtime_);
        return *runtime_;
    }

    [[nodiscard]]
    bool has_solution() const noexcept
    {
        return static_cast<bool>(solution_);
    }

    void use_initial_solution()
        requires supports_initial_solution
    {
        assert(runtime_);
        solution_ = std::make_unique<solution_type>(
            runtime_->solution_manager().initial_solution());
        clear_move_state();
    }

    void use_random_solution(rng_type& rng)
        requires supports_random_solution
    {
        assert(runtime_);
        solution_ = std::make_unique<solution_type>(
            runtime_->solution_manager().random_solution(rng));
        clear_move_state();
    }

    void set_solution(solution_type solution)
    {
        assert(runtime_);
        solution_ = std::make_unique<solution_type>(std::move(solution));
        clear_move_state();
    }

    void load_solution(std::istream& in)
        requires supports_solution_loading
    {
        assert(input_);
        auto solution = detail::tester_io::read_solution<solution_type>(
            *input_,
            in);
        detail::tester_io::require_read_success(in, "Solution");
        set_solution(std::move(solution));
    }

    void load_solution(const std::filesystem::path& path)
        requires supports_solution_loading
    {
        std::ifstream in{path};
        if (!in)
        {
            throw std::runtime_error{
                "failed to open Solution file: " + path.string()};
        }
        load_solution(in);
    }

    void save_solution(std::ostream& out) const
        requires supports_solution_saving
    {
        assert(input_);
        assert(solution_);
        detail::tester_io::write_solution(*input_, *solution_, out);
        detail::tester_io::require_write_success(out, "Solution");
    }

    void save_solution(const std::filesystem::path& path) const
        requires supports_solution_saving
    {
        std::ofstream out{path};
        if (!out)
        {
            throw std::runtime_error{
                "failed to open Solution file: " + path.string()};
        }
        save_solution(out);
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
        assert(runtime_);
        assert(solution_);
        return static_cast<bool>(
            runtime_->solution_manager().is_valid(*solution_));
    }

    [[nodiscard]]
    cost_type evaluate() const
    {
        assert(runtime_);
        assert(solution_);
        assert(is_valid());
        return runtime_->solution_manager().evaluate(*solution_);
    }

    [[nodiscard]]
    app_check_report check() const
    {
        assert(input_);
        assert(solution_);
        return easylocal::check(app_, *input_, *solution_);
    }

    [[nodiscard]]
    std::vector<std::string_view> runner_names() const
    {
        std::vector<std::string_view> names;
        names.reserve(App::runner_count);
        app_.for_each_runner_registration(
            [&]<class Algorithm>(
                const std::string_view name,
                const typename Algorithm::parameters_type&) {
                names.push_back(name);
            });
        return names;
    }

    [[nodiscard]]
    bool run_runner(const std::string_view name)
    {
        assert(runtime_);
        assert(solution_);
        assert(is_valid());

        bool found = false;
        app_.for_each_runner_registration_indexed(
            [&]<class Algorithm, std::size_t Index>(
                const std::string_view registered_name,
                const typename Algorithm::parameters_type&) {
                if (found || registered_name != name)
                {
                    return;
                }

                auto result =
                    app_.template run_at_with_rng<Index>(*input_, *solution_, rng_);
                static_assert(
                    easylocal::search_result_for<
                        decltype(result), solution_type, cost_type>,
                    "Tester requires runner results to provide the solution "
                    "and its cost (see easylocal::search_result_for)");

                solution_ = std::make_unique<solution_type>(
                    std::move(result.solution));
                clear_move_state();
                found = true;
            });
        return found;
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
        assert(runtime_);
        assert(solution_);
        return select_deterministic_move(0);
    }

    [[nodiscard]]
    bool use_next_move()
        requires supports_deterministic_moves
    {
        assert(runtime_);
        assert(solution_);

        if (!deterministic_move_index_)
        {
            return false;
        }

        return select_deterministic_move(*deterministic_move_index_ + 1);
    }

    [[nodiscard]]
    bool use_first_improving_move()
        requires supports_improvement_selection
    {
        assert(runtime_);
        assert(solution_);

        const auto current = evaluate();
        std::size_t index = 0;
        for (auto&& candidate : easylocal::moves(runtime_->neighborhood(), *solution_))
        {
            move_.emplace(candidate);
            deterministic_move_index_ = index;
            if (move_is_valid() &&
                cost::better(runtime_->solution_manager(), evaluate_move(), current))
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
        assert(runtime_);
        assert(solution_);

        std::optional<move_type> best_move;
        std::optional<cost_type> best_cost;
        std::optional<std::size_t> best_index;
        std::size_t index = 0;
        for (auto&& candidate : easylocal::moves(runtime_->neighborhood(), *solution_))
        {
            move_.emplace(candidate);
            deterministic_move_index_ = index;
            if (move_is_valid())
            {
                auto candidate_cost = evaluate_move();
                if (!best_cost || cost::better(
                        runtime_->solution_manager(), candidate_cost, *best_cost))
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
        assert(runtime_);
        assert(solution_);

        auto selected = easylocal::random_move(
            runtime_->neighborhood(),
            *solution_,
            rng);

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
        assert(runtime_);
        assert(solution_);
        assert(move_);
        return static_cast<bool>(
            runtime_->neighborhood().is_valid(*solution_, *move_));
    }

    [[nodiscard]]
    cost_type evaluate_move() const
    {
        assert(runtime_);
        assert(solution_);
        assert(move_);
        assert(move_is_valid());

        const auto& solution_manager = runtime_->solution_manager();
        const auto& neighborhood = runtime_->neighborhood();
        const detail::evaluation_facility<
            solution_manager_type,
            neighborhood_type> evaluation{
                solution_manager,
                neighborhood,
            };
        const auto current = evaluation.evaluate(*solution_);
        const auto candidate = evaluation.evaluate_move(
            *solution_,
            current,
            *move_);
        return candidate.cost();
    }

    [[nodiscard]]
    cost_type evaluate_move_fully() const
    {
        assert(runtime_);
        assert(solution_);
        assert(move_);
        assert(move_is_valid());

        auto candidate = *solution_;
        runtime_->neighborhood().make_move(candidate, *move_);
        assert(runtime_->solution_manager().is_valid(candidate));
        return runtime_->solution_manager().evaluate(candidate);
    }

    [[nodiscard]]
    bool move_evaluation_matches_full() const
        requires cost::has_equivalent<solution_manager_type>
    {
        const auto incremental = evaluate_move();
        const auto full = evaluate_move_fully();
        return cost::equivalent(
            runtime_->solution_manager(),
            incremental,
            full);
    }

    [[nodiscard]]
    neighborhood_preview_result neighborhood_preview(
        const std::size_t max_entries = 8) const
        requires supports_deterministic_moves
    {
        assert(runtime_);
        assert(solution_);

        neighborhood_preview_result result;
        const auto& solution_manager = runtime_->solution_manager();
        const auto& neighborhood = runtime_->neighborhood();
        const detail::evaluation_facility<
            solution_manager_type,
            neighborhood_type> evaluation{solution_manager, neighborhood};
        const auto current = evaluation.evaluate(*solution_);

        for (auto&& raw_move : easylocal::moves(neighborhood, *solution_))
        {
            move_type candidate{raw_move};
            ++result.moves;
            if (result.entries.size() == max_entries ||
                !static_cast<bool>(neighborhood.is_valid(*solution_, candidate)))
            {
                continue;
            }
            auto evaluated = evaluation.evaluate_move(*solution_, current, candidate);
            result.entries.push_back(inspected_move{
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
        assert(runtime_);
        assert(solution_);

        neighborhood_statistics_result result;
        const auto& solution_manager = runtime_->solution_manager();
        const auto& neighborhood = runtime_->neighborhood();
        const detail::evaluation_facility<
            solution_manager_type,
            neighborhood_type> evaluation{solution_manager, neighborhood};
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
            {
                ++result.improving;
            }
            else if (cost::better(solution_manager, current.cost(), candidate_cost))
            {
                ++result.worsening;
            }
            else
            {
                ++result.sideways;
            }
        }
        return result;
    }

    [[nodiscard]]
    neighborhood_cost_check_result check_neighborhood_costs() const
        requires supports_cost_consistency_check
    {
        assert(runtime_);
        assert(solution_);

        neighborhood_cost_check_result result;
        const auto& solution_manager = runtime_->solution_manager();
        const auto& neighborhood = runtime_->neighborhood();
        const detail::evaluation_facility<
            solution_manager_type,
            neighborhood_type> evaluation{solution_manager, neighborhood};
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
            {
                ++result.mismatches;
            }
        }
        return result;
    }

    [[nodiscard]]
    move_independence_result check_move_independence() const
        requires supports_move_independence_check
    {
        assert(runtime_);
        assert(solution_);

        move_independence_result result;
        const auto& neighborhood = runtime_->neighborhood();
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
            {
                ++result.repeated_states;
            }
            else
            {
                reached.push_back(std::move(candidate));
            }
        }
        return result;
    }

    [[nodiscard]]
    random_distribution_result check_random_move_distribution(
        rng_type& rng,
        const std::size_t rounds_per_move = 20) const
        requires supports_random_distribution_check
    {
        assert(runtime_);
        assert(solution_);

        random_distribution_result result;
        const auto& neighborhood = runtime_->neighborhood();
        std::vector<move_type> moves_list;
        for (auto&& raw_move : easylocal::moves(neighborhood, *solution_))
        {
            move_type move{raw_move};
            if (static_cast<bool>(neighborhood.is_valid(*solution_, move)))
            {
                moves_list.push_back(std::move(move));
            }
        }

        result.neighborhood_size = moves_list.size();
        if (moves_list.empty() || rounds_per_move == 0)
        {
            return result;
        }

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
            {
                ++result.out_of_neighborhood;
            }
        }

        result.min_frequency = frequencies.front();
        result.max_frequency = frequencies.front();
        for (const auto frequency : frequencies)
        {
            if (frequency == 0)
            {
                ++result.unseen;
            }
            result.min_frequency = std::min(result.min_frequency, frequency);
            result.max_frequency = std::max(result.max_frequency, frequency);
        }
        return result;
    }

    void apply_move()
    {
        assert(runtime_);
        assert(solution_);
        assert(move_);
        assert(move_is_valid());

        runtime_->neighborhood().make_move(*solution_, *move_);
        assert(runtime_->solution_manager().is_valid(*solution_));
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
        for (auto&& candidate : easylocal::moves(
                 runtime_->neighborhood(),
                 *solution_))
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
        {
            clear_move_state();
        }
        return false;
    }

    App app_;
    std::shared_ptr<const input_type> input_;
    std::unique_ptr<runtime_type> runtime_;
    std::unique_ptr<solution_type> solution_;
    std::optional<move_type> move_;
    std::optional<std::size_t> deterministic_move_index_;
    rng_type rng_;
};

} // namespace easylocal
