#pragma once

/// \file
/// termination_reason: why a search run ended, and its readable name.
///
/// A run's result reports it (search_result::termination), and so does the
/// trace (event::run_finished).

#include <string_view>

namespace easylocal
{

/// Why a search run ended.
enum class termination_reason
{
    /// The algorithm ended on its own terms (an iteration budget, a schedule
    /// that finished).
    completed,
    /// No move improves the current solution, or the neighborhood is empty.
    local_optimum,
    /// The evaluation budget was spent.
    evaluation_budget_exhausted,
    /// The run was cancelled from outside, through its run control.
    cancelled,
    /// The best cost reached the target cost.
    target_reached,
    /// Too many iterations went without improvement.
    idle_limit_reached,
    /// The time limit of the run (run_options::timeout) passed.
    time_limit_reached,
};

/// A readable name of the reason, e.g. "evaluation budget exhausted".
[[nodiscard]]
constexpr std::string_view to_string(const termination_reason reason) noexcept
{
    switch (reason)
    {
    case termination_reason::completed:
        return "completed";
    case termination_reason::local_optimum:
        return "local optimum";
    case termination_reason::evaluation_budget_exhausted:
        return "evaluation budget exhausted";
    case termination_reason::cancelled:
        return "cancelled";
    case termination_reason::target_reached:
        return "target reached";
    case termination_reason::idle_limit_reached:
        return "idle limit reached";
    case termination_reason::time_limit_reached:
        return "time limit reached";
    }
    return "unknown";
}

} // namespace easylocal
