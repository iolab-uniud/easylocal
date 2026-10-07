// A union may hold the same explorer type twice, but then child<NHE>() is
// ambiguous: the child is named by its position, child<I>().
#include <easylocal/easylocal.hpp>

#include <utility>

struct Instance
{
};
struct Solution
{
};
struct Move
{
};

struct SM
{
    using input_type = Instance;
    using solution_type = Solution;
    explicit SM(const Instance& instance) : instance_{instance} {}
    const Instance& input() const
    {
        return instance_;
    }
    static bool is_valid(const Solution&)
    {
        return true;
    }

private:
    const Instance& instance_;
};

struct NHE : easylocal::neighborhood_explorer_base<SM, Move>
{
    using neighborhood_explorer_base::neighborhood_explorer_base;
    static bool is_valid(const Solution&, const Move&)
    {
        return true;
    }
    static void make_move(Solution&, const Move&) {}
    static auto moves(const Solution&)
    {
        return std::views::empty<Move>;
    }
};

using Spec = decltype(easylocal::neighborhood_union(
    easylocal::neighborhood<NHE>(),
    easylocal::neighborhood<NHE>()));
using Union = typename Spec::service_type;
using Ambiguous = decltype(std::declval<Union&>().template child<NHE>());
