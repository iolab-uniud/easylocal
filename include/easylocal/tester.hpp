#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <random>
#include <utility>

#include <easylocal/check.hpp>
#include <easylocal/cursor_moves.hpp>
#include <easylocal/detail/cost_semantics.hpp>
#include <easylocal/detail/evaluation.hpp>
#include <easylocal/detail/solution_manager_concepts.hpp>

namespace easylocal
{

template<class App>
    requires std::move_constructible<App> &&
             requires { typename App::input_type; }
class Tester
{
public:
    using app_type = App;
    using input_type = typename App::input_type;
    using instance_type = decltype(
        std::declval<const App&>().for_input(
            std::declval<const input_type&>()));
    using solution_manager_type = typename instance_type::solution_manager_type;
    using neighborhood_type = typename instance_type::neighborhood_explorer_type;
    using solution_type = typename solution_manager_type::solution_type;
    using cost_type = typename solution_manager_type::cost_type;
    using move_type = typename neighborhood_type::move_type;
    using rng_type = std::mt19937_64;

    static constexpr bool supports_initial_solution =
        detail::has_initial_solution<solution_manager_type>;
    static constexpr bool supports_random_solution =
        detail::has_random_solution<solution_manager_type, rng_type>;
    static constexpr bool supports_deterministic_moves =
        deterministic_neighborhood_for<neighborhood_type, solution_type>;
    static constexpr bool supports_random_moves =
        random_neighborhood_for<neighborhood_type, solution_type, rng_type>;

    static_assert(
        supports_initial_solution || supports_random_solution,
        "Tester requires the SolutionManager to provide initial_solution() "
        "or random_solution(std::mt19937_64&)");

    explicit Tester(App application)
        : app_{std::move(application)}
    {
    }

    [[nodiscard]]
    auto app() noexcept -> App&
    {
        return app_;
    }

    [[nodiscard]]
    auto app() const noexcept -> const App&
    {
        return app_;
    }

    [[nodiscard]]
    auto has_input() const noexcept -> bool
    {
        return static_cast<bool>(input_);
    }

    void set_input(input_type input)
    {
        auto new_input = std::make_unique<input_type>(std::move(input));
        auto new_instance = std::unique_ptr<instance_type>{
            new instance_type(app_.for_input(*new_input))};

        clear_solution_state();
        instance_.reset();
        input_ = std::move(new_input);
        instance_ = std::move(new_instance);
    }

    [[nodiscard]]
    auto input() const noexcept -> const input_type&
    {
        assert(input_);
        return *input_;
    }

    [[nodiscard]]
    auto instance() noexcept -> instance_type&
    {
        assert(instance_);
        return *instance_;
    }

    [[nodiscard]]
    auto instance() const noexcept -> const instance_type&
    {
        assert(instance_);
        return *instance_;
    }

    [[nodiscard]]
    auto has_solution() const noexcept -> bool
    {
        return static_cast<bool>(solution_);
    }

    void use_initial_solution()
        requires supports_initial_solution
    {
        assert(instance_);
        solution_ = std::make_unique<solution_type>(
            instance_->solution_manager().initial_solution());
        clear_move_state();
    }

    void use_random_solution(rng_type& rng)
        requires supports_random_solution
    {
        assert(instance_);
        solution_ = std::make_unique<solution_type>(
            instance_->solution_manager().random_solution(rng));
        clear_move_state();
    }

    void set_solution(solution_type solution)
    {
        assert(instance_);
        solution_ = std::make_unique<solution_type>(std::move(solution));
        clear_move_state();
    }

    [[nodiscard]]
    auto solution() const noexcept -> const solution_type&
    {
        assert(solution_);
        return *solution_;
    }

    [[nodiscard]]
    auto is_valid() const -> bool
    {
        assert(instance_);
        assert(solution_);
        return static_cast<bool>(
            instance_->solution_manager().is_valid(*solution_));
    }

    [[nodiscard]]
    auto evaluate() const -> cost_type
    {
        assert(instance_);
        assert(solution_);
        assert(is_valid());
        return instance_->solution_manager().evaluate(*solution_);
    }

    [[nodiscard]]
    auto check() const -> app_check_report
    {
        assert(input_);
        assert(solution_);
        return easylocal::check(app_, *input_, *solution_);
    }

    [[nodiscard]]
    auto has_move() const noexcept -> bool
    {
        return move_.has_value();
    }

    [[nodiscard]]
    auto move() const noexcept -> const move_type&
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
    auto use_first_move() -> bool
        requires supports_deterministic_moves
    {
        assert(instance_);
        assert(solution_);
        return select_deterministic_move(0);
    }

    [[nodiscard]]
    auto use_next_move() -> bool
        requires supports_deterministic_moves
    {
        assert(instance_);
        assert(solution_);

        if (!deterministic_move_index_)
        {
            return false;
        }

        return select_deterministic_move(*deterministic_move_index_ + 1);
    }

    [[nodiscard]]
    auto use_random_move(rng_type& rng) -> bool
        requires supports_random_moves
    {
        assert(instance_);
        assert(solution_);

        auto selected = easylocal::random_move(
            instance_->neighborhood(),
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
    auto move_is_valid() const -> bool
    {
        assert(instance_);
        assert(solution_);
        assert(move_);
        return static_cast<bool>(
            instance_->neighborhood().is_valid(*solution_, *move_));
    }

    [[nodiscard]]
    auto evaluate_move() const -> cost_type
    {
        assert(instance_);
        assert(solution_);
        assert(move_);
        assert(move_is_valid());

        const auto& solution_manager = instance_->solution_manager();
        const auto& neighborhood = instance_->neighborhood();
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
    auto evaluate_move_fully() const -> cost_type
    {
        assert(instance_);
        assert(solution_);
        assert(move_);
        assert(move_is_valid());

        auto candidate = *solution_;
        instance_->neighborhood().make_move(candidate, *move_);
        assert(instance_->solution_manager().is_valid(candidate));
        return instance_->solution_manager().evaluate(candidate);
    }

    [[nodiscard]]
    auto move_evaluation_matches_full() const -> bool
        requires detail::has_equivalent<solution_manager_type>
    {
        const auto incremental = evaluate_move();
        const auto full = evaluate_move_fully();
        return detail::cost_equivalent(
            instance_->solution_manager(),
            incremental,
            full);
    }

    void apply_move()
    {
        assert(instance_);
        assert(solution_);
        assert(move_);
        assert(move_is_valid());

        instance_->neighborhood().make_move(*solution_, *move_);
        assert(instance_->solution_manager().is_valid(*solution_));
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
    auto select_deterministic_move(const std::size_t target) -> bool
        requires supports_deterministic_moves
    {
        std::size_t index = 0;
        for (auto&& candidate : easylocal::moves(
                 instance_->neighborhood(),
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
    std::unique_ptr<input_type> input_;
    std::unique_ptr<instance_type> instance_;
    std::unique_ptr<solution_type> solution_;
    std::optional<move_type> move_;
    std::optional<std::size_t> deterministic_move_index_;
};

} // namespace easylocal
