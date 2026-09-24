#pragma once

namespace easylocal
{

template<class Instance, class Solution>
class solution_manager_base
{
public:
    using instance_type = Instance;
    using solution_type = Solution;

    explicit solution_manager_base(const instance_type& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const instance_type&
    {
        return instance_;
    }

protected:
    const instance_type& instance_;
};

template<class SolutionManager, class Move>
class neighborhood_explorer_base
{
public:
    using solution_manager_type = SolutionManager;
    using instance_type = typename solution_manager_type::instance_type;
    using solution_type = typename solution_manager_type::solution_type;
    using move_type = Move;

    explicit neighborhood_explorer_base(
        const solution_manager_type& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto instance() const noexcept -> const instance_type&
    {
        return solution_manager_.instance();
    }

protected:
    const solution_manager_type& solution_manager_;
};

} // namespace easylocal
