#pragma once

namespace easylocal
{

template<class Instance, class Solution>
class solution_manager_base
{
public:
    using input_type = Instance;
    using instance_type = input_type;
    using solution_type = Solution;

    explicit solution_manager_base(const instance_type& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto input() const noexcept -> const input_type&
    {
        return instance_;
    }

    [[nodiscard]]
    auto instance() const noexcept -> const instance_type&
    {
        return input();
    }

protected:
    const instance_type& instance_;
};

template<class SolutionManager, class Move>
class neighborhood_explorer_base
{
public:
    using solution_manager_type = SolutionManager;
    using input_type = typename solution_manager_type::instance_type;
    using instance_type = input_type;
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
        return solution_manager_.instance();
    }

    [[nodiscard]]
    auto instance() const noexcept -> const instance_type&
    {
        return input();
    }

protected:
    const solution_manager_type& solution_manager_;
};

} // namespace easylocal
