#pragma once

namespace easylocal
{

template<class Input, class Solution>
class solution_manager_base
{
public:
    using input_type = Input;
    using solution_type = Solution;

    explicit solution_manager_base(const input_type& input) noexcept
        : input_{input}
    {
    }

    [[nodiscard]]
    auto input() const noexcept -> const input_type&
    {
        return input_;
    }

protected:
    const input_type& input_;
};

template<class SolutionManager, class Move>
class neighborhood_explorer_base
{
public:
    using solution_manager_type = SolutionManager;
    using input_type = typename solution_manager_type::input_type;
    using solution_type = typename solution_manager_type::solution_type;
    using move_type = Move;

    explicit neighborhood_explorer_base(
        const solution_manager_type& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto input() const noexcept -> const input_type&
    {
        return solution_manager_.input();
    }

protected:
    const solution_manager_type& solution_manager_;
};

} // namespace easylocal
