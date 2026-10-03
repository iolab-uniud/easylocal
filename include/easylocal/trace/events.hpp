#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

// Typed semantic search events and hierarchical neighborhood provenance.
namespace easylocal::trace
{

struct neighborhood_route_node
{
    std::size_t child{};
    const neighborhood_route_node* parent{};
};

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

template<class Cost>
struct run_started
{
    Cost cost;
};

template<class Cost>
struct move_evaluated
{
    std::size_t evaluations{};
    std::size_t iterations{};
    Cost current_cost;
    Cost candidate_cost;
    const neighborhood_route_node* neighborhood{};
};

template<class Cost>
struct move_accepted
{
    std::size_t evaluations{};
    std::size_t iterations{};
    Cost previous_cost;
    Cost cost;
    const neighborhood_route_node* neighborhood{};
};

template<class Cost>
struct incumbent_updated
{
    std::size_t evaluations{};
    std::size_t iterations{};
    Cost previous_cost;
    Cost cost;
};

template<class Cost>
struct local_optimum
{
    std::size_t evaluations{};
    std::size_t iterations{};
    Cost cost;
};

struct neighborhood_selection
{
    std::size_t attempt{};
    std::size_t child{};
    double bias{};
    double active_bias_total{};
    double conditional_probability{};
    bool produced_move{};
    const neighborhood_route_node* neighborhood{};
};

template<class Cost>
struct run_finished
{
    std::size_t evaluations{};
    std::size_t iterations{};
    Cost cost;
};

} // namespace event

} // namespace easylocal::trace
