#pragma once

/// \file
/// pareto_archive: the non-dominated solutions (Pareto front) reached by a
/// search with a cost::pareto cost, kept by search_run and returned in the
/// pareto_search_result.

#include <easylocal/cost/pareto.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/utils/limit.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <stdexcept>
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

/// The parameters of a pareto_archive: what it keeps of solutions of
/// equivalent cost, and how many points.
struct pareto_archive_parameters
{
    /// Whether every distinct solution of an equivalent cost is kept (default
    /// false: one point per cost, the first reached).
    bool keep_equivalent{false};
    /// The most points the archive holds, at least 1 (default unlimited): once
    /// it is full, a solution enters only by removing the points it dominates.
    limit max_front_size{unlimited};
};

namespace detail
{

// The relations an archive compares costs with: those of a search (a run, a
// bound runner) when it has them, otherwise the cost type's < and ==.
template<class Relations, class Cost>
[[nodiscard]]
bool archive_better(
    const Relations& relations,
    const Cost& candidate,
    const Cost& reference)
{
    if constexpr (requires { relations.better(candidate, reference); })
        return static_cast<bool>(relations.better(candidate, reference));
    else
        return static_cast<bool>(candidate < reference);
}

template<class Relations, class Cost>
[[nodiscard]]
bool archive_equivalent(const Relations& relations, const Cost& lhs, const Cost& rhs)
{
    if constexpr (requires { relations.equivalent(lhs, rhs); })
        return static_cast<bool>(relations.equivalent(lhs, rhs));
    else
        return static_cast<bool>(lhs == rhs);
}

// The relations of the cost type itself, for an archive used outside a search.
struct intrinsic_cost_relations
{
};

} // namespace detail

/// The non-dominated solutions reached by a search with a cost::pareto cost: a
/// solution enters unless an archived one is better or, by default, has an
/// equivalent cost, and removes the archived solutions it is better than.
///
/// The costs are compared with the relations of the search (its better() and
/// equivalent(), which a root `compare` of the cost expression defines), so
/// the archive agrees with the runner. The parameters may keep every distinct
/// solution of an equivalent cost, and bound the number of points.
template<class Solution, class Cost>
class pareto_archive
{
public:
    /// A solution of the archive with its cost.
    using point_type = pareto_point<Solution, Cost>;
    /// The parameter block of the archive.
    using parameters_type = pareto_archive_parameters;

    /// An archive that keeps one point per non-dominated cost.
    pareto_archive() = default;

    /// From its parameters.
    ///
    /// Throws `std::invalid_argument` when `max_front_size` is 0.
    explicit pareto_archive(const pareto_archive_parameters parameters)
        : parameters_{parameters}
    {
        if (parameters_.max_front_size == 0)
            throw std::invalid_argument{"the front must hold at least one point"};
    }

    /// The parameters.
    [[nodiscard]]
    const pareto_archive_parameters& parameters() const noexcept
    {
        return parameters_;
    }

    /// Offers a solution with its cost, compared with the better() and
    /// equivalent() of `relations` (a run, a bound runner), or the cost's `<`
    /// and `==` when it lacks them; `same_solution(lhs, rhs)` tells whether
    /// two solutions of equivalent cost are the same one, when the archive
    /// keeps them all.
    ///
    /// True when the solution entered the archive.
    template<class Relations, class SameSolution>
    bool offer(
        const Solution& solution,
        const Cost& cost,
        const Relations& relations,
        SameSolution&& same_solution)
    {
        for (const auto& point : points_)
        {
            if (detail::archive_better(relations, point.cost, cost))
                return false;
            if (detail::archive_equivalent(relations, point.cost, cost)
                && (!parameters_.keep_equivalent
                    || same_solution(point.solution, solution)))
                return false;
        }
        std::erase_if(points_, [&](const point_type& point) {
            return detail::archive_better(relations, cost, point.cost);
        });
        if (points_.size() >= parameters_.max_front_size)
            return false;
        points_.push_back(point_type{solution, cost});
        return true;
    }

    /// Offers a solution with its cost, compared with the cost's `<` and `==`.
    ///
    /// True when the solution entered the archive.
    template<class SameSolution>
    bool offer(const Solution& solution, const Cost& cost, SameSolution&& same_solution)
    {
        return offer(
            solution,
            cost,
            detail::intrinsic_cost_relations{},
            std::forward<SameSolution>(same_solution));
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

    /// The point that sorted() puts first, found without sorting; the archive
    /// must not be empty.
    [[nodiscard]]
    const point_type& first() const noexcept
    {
        assert(!points_.empty());
        return *std::ranges::min_element(
            points_,
            [](const point_type& lhs, const point_type& rhs) {
                return objectives_less(lhs.cost, rhs.cost);
            });
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

    pareto_archive_parameters parameters_{};
    std::vector<point_type> points_;
};

namespace detail
{

// Whether two solutions of equivalent cost are the same one, for
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
