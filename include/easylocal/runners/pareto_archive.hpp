#pragma once

/// \file
/// pareto_archive: the non-dominated solutions (Pareto front) reached by a
/// search with a cost::pareto cost, kept by search_run and returned in the
/// pareto_search_result.

#include <easylocal/cost/pareto.hpp>
#include <easylocal/helpers/solution_manager.hpp>

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal
{

/// A solution of a Pareto front, with its cost.
template<class Solution, class Cost>
struct pareto_point
{
    /// The solution.
    Solution solution;
    /// Its cost.
    Cost cost;
};

/// The non-dominated solutions reached by a search with a cost::pareto cost: a
/// solution enters unless an archived one dominates it or is the same (equal
/// cost and the same solution), and removes the archived solutions it
/// dominates.
///
/// Solutions with equal costs are all kept when they differ.
template<class Solution, class Cost>
class pareto_archive
{
public:
    /// A solution of the archive with its cost.
    using point_type = pareto_point<Solution, Cost>;

    /// same_solution(lhs, rhs) tells whether two solutions of equal cost are
    /// the same one.
    ///
    /// True when the solution entered the archive.
    template<class SameSolution>
    bool offer(const Solution& solution, const Cost& cost, SameSolution&& same_solution)
    {
        for (const auto& point : points_)
        {
            if (point.cost < cost)
                return false;
            if (point.cost == cost && same_solution(point.solution, solution))
                return false;
        }
        std::erase_if(points_, [&cost](const point_type& point) {
            return cost < point.cost;
        });
        points_.push_back(point_type{solution, cost});
        return true;
    }

    /// The points, in the order they entered.
    [[nodiscard]]
    const std::vector<point_type>& points() const noexcept
    {
        return points_;
    }

    /// The number of points.
    [[nodiscard]]
    std::size_t size() const noexcept
    {
        return points_.size();
    }

    /// Removes every point.
    void clear() noexcept
    {
        points_.clear();
    }

    /// The points ordered by their objectives, first objective first.
    [[nodiscard]]
    std::vector<point_type> sorted() const
    {
        auto result = points_;
        std::ranges::sort(result, [](const point_type& lhs, const point_type& rhs) {
            return objectives_less(lhs.cost, rhs.cost);
        });
        return result;
    }

private:
    [[nodiscard]]
    static bool objectives_less(const Cost& lhs, const Cost& rhs)
    {
        return [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            bool decided = false;
            bool less = false;
            const auto compare = [&](const auto& left, const auto& right) {
                if (decided)
                    return;
                if (left < right)
                {
                    decided = true;
                    less = true;
                }
                else if (right < left)
                {
                    decided = true;
                }
            };
            (compare(lhs.template get<Index>(), rhs.template get<Index>()), ...);
            return less;
        }(std::make_index_sequence<Cost::levels>{});
    }

    std::vector<point_type> points_;
};

namespace detail
{

// Whether two solutions of equal cost are the same one, for
// pareto_archive::offer: the SolutionManager's solution equality when owner
// (a search context or a bound runner) exposes a solution_manager() that has
// one, otherwise always (one point per cost).
template<class Owner>
[[nodiscard]]
auto same_solution_of(const Owner& owner) noexcept
{
    return [&owner]<class Solution>(const Solution& lhs, const Solution& rhs) -> bool {
        if constexpr (requires {
                          requires has_solution_equality<
                              std::remove_cvref_t<decltype(owner.solution_manager())>>;
                      })
            return easylocal::solutions_equal(owner.solution_manager(), lhs, rhs);
        else
        {
            static_cast<void>(owner); // used only by problems with equality
            return true;
        }
    };
}

} // namespace detail

} // namespace easylocal
