#include <easylocal/runner.hpp>
#include <easylocal/search/detail/context_concepts.hpp>
#include <easylocal/search/simulated_annealing.hpp>

#include <optional>
#include <random>
#include <ranges>
#include <utility>

namespace
{

struct Instance
{
};

struct Solution
{
};

struct Move
{
};

struct WrongMove
{
};

struct Cost
{
};

class SolutionManager
{
public:
    using instance_type = Instance;
    using solution_type = Solution;
    using cost_type = Cost;

    [[nodiscard]]
    auto instance() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]]
    static auto is_valid(const Solution&) noexcept -> bool
    {
        return true;
    }

    [[nodiscard]]
    static auto evaluate(const Solution&) noexcept -> Cost
    {
        return {};
    }

private:
    Instance instance_;
};

class GoodNeighborhood
{
public:
    using instance_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    [[nodiscard]]
    auto instance() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]]
    static auto moves(const Solution&)
    {
        return std::views::single(Move{});
    }

    [[nodiscard]] static auto is_valid(const Solution&, const Move&) noexcept -> bool { return true; }

    static void make_move(Solution&, const Move&) noexcept
    {
    }

private:
    Instance instance_;
};

class WrongMoveNeighborhood
{
public:
    using instance_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    [[nodiscard]]
    auto instance() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]]
    static auto moves(const Solution&)
    {
        return std::views::single(WrongMove{});
    }

    [[nodiscard]] static auto is_valid(const Solution&, const Move&) noexcept -> bool { return true; }

    static void make_move(Solution&, const Move&) noexcept
    {
    }

private:
    Instance instance_;
};



class RandomMoveGoodNeighborhood : public GoodNeighborhood
{
public:
    [[nodiscard]]
    static auto random_move(const Solution&, std::mt19937&)
        -> std::optional<Move>
    {
        return Move{};
    }
};

class RandomMoveOnlyNeighborhood
{
public:
    using instance_type = Instance;
    using solution_type = Solution;
    using move_type = Move;

    [[nodiscard]]
    auto instance() const noexcept -> const Instance&
    {
        return instance_;
    }

    [[nodiscard]]
    static auto random_move(const Solution&, std::mt19937&)
        -> std::optional<Move>
    {
        return Move{};
    }

    [[nodiscard]] static auto is_valid(const Solution&, const Move&) noexcept -> bool { return true; }

    static void make_move(Solution&, const Move&) noexcept
    {
    }

private:
    Instance instance_;
};


struct EvaluationState
{
    Cost value{};

    [[nodiscard]]
    auto cost() const noexcept -> const Cost&
    {
        return value;
    }
};

struct CandidateState
{
    Cost value{};

    [[nodiscard]]
    auto cost() const noexcept -> const Cost&
    {
        return value;
    }
};

class Evaluation
{
public:
    using evaluation_type = EvaluationState;
    using candidate_type = CandidateState;

    [[nodiscard]]
    static auto evaluate(const Solution&) noexcept -> evaluation_type
    {
        return {};
    }

    [[nodiscard]]
    static auto evaluate_move(
        const Solution&,
        const evaluation_type&,
        const Move&) noexcept -> candidate_type
    {
        return {};
    }

    static void commit(
        Solution&,
        evaluation_type&,
        candidate_type&&) noexcept
    {
    }
};

template<class Neighborhood>
class SearchContext
{
public:
    using solution_type = Solution;
    using cost_type = Cost;
    using neighborhood_explorer_type = Neighborhood;

    [[nodiscard]]
    auto neighborhood_explorer() const noexcept
        -> const neighborhood_explorer_type&
    {
        return neighborhood_;
    }

    [[nodiscard]]
    static auto evaluation() noexcept -> Evaluation
    {
        return {};
    }

    [[nodiscard]]
    static auto better(const Cost&, const Cost&) noexcept -> bool
    {
        return false;
    }

private:
    Neighborhood neighborhood_;
};

} // namespace

int main()
{
    static_assert(easylocal::detail::runner_solution_manager<SolutionManager>);
    static_assert(easylocal::detail::runner_neighborhood_explorer<
                  GoodNeighborhood,
                  SolutionManager>);
    static_assert(easylocal::detail::runner_neighborhood_explorer<
                  WrongMoveNeighborhood,
                  SolutionManager>);
    static_assert(easylocal::detail::enumerable_runner_neighborhood_explorer<
                  GoodNeighborhood,
                  SolutionManager>);
    static_assert(!easylocal::detail::enumerable_runner_neighborhood_explorer<
                  WrongMoveNeighborhood,
                  SolutionManager>);
    static_assert(easylocal::detail::runner_neighborhood_explorer<
                  RandomMoveOnlyNeighborhood,
                  SolutionManager>);
    static_assert(!easylocal::detail::enumerable_runner_neighborhood_explorer<
                  RandomMoveOnlyNeighborhood,
                  SolutionManager>);

    static_assert(easylocal::search::detail::strict_improvement_context<
                  SearchContext<GoodNeighborhood>>);
    static_assert(easylocal::search::detail::strict_improvement_context<
                  SearchContext<WrongMoveNeighborhood>>);
    static_assert(easylocal::search::detail::enumerating_strict_improvement_context<
                  SearchContext<GoodNeighborhood>>);
    static_assert(!easylocal::search::detail::enumerating_strict_improvement_context<
                  SearchContext<WrongMoveNeighborhood>>);

    static_assert(easylocal::search::detail::random_move_context<
                  SearchContext<RandomMoveGoodNeighborhood>,
                  std::mt19937>);

    return 0;
}
