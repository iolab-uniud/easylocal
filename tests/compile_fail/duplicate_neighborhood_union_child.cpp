#include <easylocal/easylocal.hpp>

struct Instance {};
struct Solution {};
struct Move {};

struct SM
{
    using input_type = Instance;
    using solution_type = Solution;
    explicit SM(const Instance& instance) : instance_{instance} {}
    auto instance() const -> const Instance& { return instance_; }
    static auto is_valid(const Solution&) -> bool { return true; }
private:
    const Instance& instance_;
};

struct NHE : easylocal::neighborhood_explorer_base<SM, Move>
{
    using neighborhood_explorer_base::neighborhood_explorer_base;
    static auto is_valid(const Solution&, const Move&) -> bool { return true; }
    static void make_move(Solution&, const Move&) {}
    static auto moves(const Solution&) { return std::views::empty<Move>; }
};

using Spec = decltype(easylocal::neighborhood_union(
    easylocal::neighborhood<NHE>(),
    easylocal::neighborhood<NHE>()));
using Broken = typename Spec::service_type;
static_assert(sizeof(Broken) != 0);
