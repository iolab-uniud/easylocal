#include <easylocal/sampling.hpp>
#include <easylocal/search/random_first_improvement.hpp>

#include <concepts>
#include <random>
#include <ranges>
#include <utility>

namespace
{

struct Solution
{
};

struct Cost
{
};

struct Candidate
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
    using evaluation_type = Candidate;
    using candidate_type = Candidate;

    [[nodiscard]]
    auto evaluate(const Solution&) const noexcept -> Candidate
    {
        return {};
    }

    [[nodiscard]]
    auto evaluate_move(
        const Solution&,
        const Candidate&,
        int) const noexcept -> Candidate
    {
        return {};
    }

    void commit(Solution&, Candidate&, Candidate&&) const noexcept
    {
    }
};

template<class Sampling>
class NeighborhoodExplorer
{
public:
    using move_type = int;
    using random_sampling = Sampling;

    [[nodiscard]]
    auto random_moves(const Solution&, std::mt19937&) const
    {
        return std::views::single(0);
    }
};

template<class Sampling>
class Context
{
public:
    using solution_type = Solution;
    using cost_type = Cost;
    using neighborhood_explorer_type = NeighborhoodExplorer<Sampling>;

    [[nodiscard]]
    auto neighborhood_explorer() const noexcept
        -> const neighborhood_explorer_type&
    {
        return neighborhood_;
    }

    [[nodiscard]]
    auto evaluation() const noexcept -> Evaluation
    {
        return {};
    }

    [[nodiscard]]
    static auto better(const Cost&, const Cost&) noexcept -> bool
    {
        return false;
    }

private:
    neighborhood_explorer_type neighborhood_;
};

template<class ContextType>
concept CanRunRandomFirstImprovement = requires(
    const easylocal::search::RandomFirstImprovement& algorithm,
    const ContextType& context,
    Solution solution,
    std::mt19937& rng)
{
    algorithm.run(context, std::move(solution), rng);
};

} // namespace

int main()
{
    static_assert(CanRunRandomFirstImprovement<
        Context<easylocal::sampling::without_replacement>>);
    static_assert(!CanRunRandomFirstImprovement<
        Context<easylocal::sampling::with_replacement>>);
    return 0;
}
