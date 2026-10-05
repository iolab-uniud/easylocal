#pragma once

/// \file
/// Typed semantic search events and hierarchical neighborhood provenance.

#include <easylocal/utils/termination.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace easylocal::trace
{

/// A level of the route of a move through nested neighborhood unions, linked to
/// the level above.
///
/// The nodes live on the stack of the search while an event is emitted: a
/// tracer that keeps the route copies it with copy_route().
struct neighborhood_route_node
{
    /// The index of the child neighborhood in its union.
    std::size_t child{};
    /// The node of the enclosing union, or null at the outermost one.
    const neighborhood_route_node* parent{};
};

/// The child indices of the route ending at node, from the outermost union
/// inward; empty for a null node.
inline std::vector<std::size_t> copy_route(const neighborhood_route_node* node)
{
    std::vector<std::size_t> route;
    for (auto* current = node; current != nullptr; current = current->parent)
    {
        route.push_back(current->child);
    }
    std::ranges::reverse(route);
    return route;
}

namespace event
{

/// The start of a run, emitted by `search_run::start()` once the initial
/// solution is evaluated.
template<class Cost>
struct run_started
{
    /// The cost of the initial solution.
    Cost cost;
};

/// A move evaluated, emitted by `search_run::evaluate_move()`.
template<class Cost>
struct move_evaluated
{
    /// Evaluations so far, this one included.
    std::size_t evaluations{};
    /// Iterations so far.
    std::size_t iterations{};
    /// The cost of the current solution.
    Cost current_cost;
    /// The cost the solution would have after the move.
    Cost candidate_cost;
    /// The route of the move through neighborhood unions, or null outside one.
    const neighborhood_route_node* neighborhood{};
};

/// A move applied to the current solution, emitted by `search_run::commit()`.
template<class Cost>
struct move_accepted
{
    /// Evaluations so far.
    std::size_t evaluations{};
    /// Iterations so far.
    std::size_t iterations{};
    /// The cost of the solution before the move.
    Cost previous_cost;
    /// The cost of the solution after the move.
    Cost cost;
    /// The route of the move through neighborhood unions, or null outside one.
    const neighborhood_route_node* neighborhood{};
};

/// A new best solution, emitted by `search_run::incumbent_updated()` in the
/// algorithms that keep the best solution found.
template<class Cost>
struct incumbent_updated
{
    /// Evaluations so far.
    std::size_t evaluations{};
    /// Iterations so far.
    std::size_t iterations{};
    /// The cost of the previous best solution.
    Cost previous_cost;
    /// The cost of the new best solution.
    Cost cost;
};

/// A run ended in a local optimum, emitted by `search_run::finish()` before
/// run_finished.
template<class Cost>
struct local_optimum
{
    /// Evaluations so far.
    std::size_t evaluations{};
    /// Iterations so far.
    std::size_t iterations{};
    /// The cost of the local optimum.
    Cost cost;
};

/// A child chosen by a neighborhood union drawing a random move, emitted once
/// the child has tried to produce one.
///
/// It describes the choice of the branch, not the probability of the move.
struct neighborhood_selection
{
    /// The attempt within the draw, from 0; a child without moves is excluded
    /// and another drawn.
    std::size_t attempt{};
    /// The index of the chosen child in its union.
    std::size_t child{};
    /// The bias of the chosen child.
    double bias{};
    /// The total bias of the children still active at this attempt.
    double active_bias_total{};
    /// The probability of choosing this child at this attempt, bias divided by
    /// active_bias_total (0 when the total is 0).
    double conditional_probability{};
    /// Whether the child produced a move.
    bool produced_move{};
    /// The route of the chosen child through the enclosing unions.
    const neighborhood_route_node* neighborhood{};
};

/// The solution reached at the start of a run and after each applied move,
/// identified by its hash (solution_hash): the nodes of search trajectory and
/// local optima networks.
///
/// Emitted only when the problem has a solution hash.
template<class Cost>
struct solution_visited
{
    /// Evaluations so far.
    std::size_t evaluations{};
    /// Iterations so far.
    std::size_t iterations{};
    /// The solution hash of the solution.
    std::uint64_t hash{};
    /// The cost of the solution.
    Cost cost;
};

/// The move just applied was tabu, admitted by the aspiration criterion.
template<class Cost>
struct aspiration_applied
{
    /// Evaluations so far.
    std::size_t evaluations{};
    /// Iterations so far.
    std::size_t iterations{};
    /// The cost of the solution after the move.
    Cost cost;
};

/// A reactive tabu list's escape, after its random moves: `moves` were
/// applied, fewer than asked when the run had to stop.
struct tabu_escape
{
    /// Evaluations so far.
    std::size_t evaluations{};
    /// Iterations so far.
    std::size_t iterations{};
    /// The number of random moves the escape applied.
    std::size_t moves{};
};

/// The tenure of a tabu list with one tenure for all its moves changed, from
/// previous_tenure (0 at the start of a run) to tenure.
struct tabu_tenure_changed
{
    /// Evaluations so far.
    std::size_t evaluations{};
    /// Iterations so far.
    std::size_t iterations{};
    /// The tenure before the change, 0 at the start of a run.
    std::size_t previous_tenure{};
    /// The new tenure.
    std::size_t tenure{};
};

/// The end of a run, emitted by `search_run::finish()`.
template<class Cost>
struct run_finished
{
    /// Evaluations in the run.
    std::size_t evaluations{};
    /// Iterations in the run.
    std::size_t iterations{};
    /// The cost of the solution returned.
    Cost cost;
    /// Why the run ended.
    termination_reason termination{termination_reason::completed};
};

} // namespace event

} // namespace easylocal::trace
