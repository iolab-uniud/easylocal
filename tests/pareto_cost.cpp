// cost::pareto, ordered by dominance, and the archive of the non-dominated
// solutions that search_run keeps for such a cost.
#include <easylocal/cost.hpp>
#include <easylocal/runners/pareto_archive.hpp>
#include <easylocal/trace/binary.hpp>

#include <compare>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{

using easylocal::cost::pareto;
using cost_type = pareto<int, double>;

auto expect(const bool condition, const std::string_view description) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }
    return true;
}

[[nodiscard]]
auto same_name(const std::string& lhs, const std::string& rhs) -> bool
{
    return lhs == rhs;
}

static_assert(easylocal::cost::pareto_type<cost_type>);
static_assert(!easylocal::cost::pareto_type<easylocal::cost::lexicographic<int, double>>);
static_assert(easylocal::cost::text_readable<cost_type>);

} // namespace

int main()
{
    bool ok = true;

    const cost_type low_first{1, 5.0};
    const cost_type low_second{3, 2.0};
    const cost_type dominated{3, 5.0};

    ok &= expect(
        low_first < dominated && low_second < dominated && !(dominated < low_first),
        "a cost better in one objective and no worse in the others dominates");
    ok &= expect(
        !(low_first < low_second) && !(low_second < low_first)
            && (low_first <=> low_second) == std::partial_ordering::unordered,
        "costs better in different objectives are unordered");
    ok &= expect(
        low_first <= cost_type{1, 5.0} && low_first == cost_type{1, 5.0}
            && !(low_first < cost_type{1, 5.0}),
        "equal costs weakly dominate each other");
    ok &= expect(
        easylocal::cost::zero<cost_type>() == cost_type{0, 0.0},
        "the zero of a pareto cost is zero in every objective");

    ok &= expect(
        easylocal::cost::to_text(low_first) == "[1, 5]"
            && easylocal::cost::from_text<cost_type>(" [3, 2.5] ") == cost_type{3, 2.5},
        "a pareto cost is read and written as [v1, v2, ...]");

    const easylocal::trace::default_binary_cost_writer<cost_type> writer;
    const auto fields = writer.fields();
    ok &= expect(
        fields.size() == 2 && fields[0].name == "0"
            && fields[0].type == easylocal::trace::binary_type::i64
            && fields[1].type == easylocal::trace::binary_type::f64,
        "ELTR describes a pareto cost by its objectives");

    {
        easylocal::pareto_archive<std::string, cost_type> archive;
        ok &= expect(
            archive.offer("dominated", dominated, same_name),
            "the first solution enters");
        ok &= expect(
            archive.offer("first", low_first, same_name) && archive.size() == 1
                && archive.points().front().solution == "first",
            "a dominating solution enters and removes the ones it dominates");
        ok &= expect(
            archive.offer("second", low_second, same_name) && archive.size() == 2,
            "an unordered solution enters");
        ok &= expect(
            !archive.offer("worse", cost_type{4, 6.0}, same_name) && archive.size() == 2,
            "a dominated solution does not enter");
        ok &= expect(
            !archive.offer("first", low_first, same_name)
                && !archive.offer("twin", low_first, same_name) && archive.size() == 2,
            "by default one point per cost: the first reached stays");
        const auto sorted = archive.sorted();
        ok &= expect(
            sorted.size() == 2 && sorted.front().cost == low_first
                && sorted.back().cost == low_second && archive.first().cost == low_first,
            "the front is ordered by its objectives");
    }

    {
        easylocal::pareto_archive<std::string, cost_type> archive{{
            .keep_equivalent = true,
            .max_front_size = 3,
        }};
        ok &= expect(
            archive.offer("first", low_first, same_name)
                && !archive.offer("first", low_first, same_name)
                && archive.offer("twin", low_first, same_name) && archive.size() == 2,
            "keep_equivalent keeps each distinct solution of an equal cost once");
        ok &= expect(
            archive.offer("second", low_second, same_name)
                && !archive.offer("triplet", low_first, same_name) && archive.size() == 3,
            "a full archive takes no point that removes none");
        ok &= expect(
            archive.offer("best", cost_type{0, 1.0}, same_name) && archive.size() == 1,
            "a dominating point enters a full archive, removing those it dominates");
        bool rejected = false;
        try
        {
            [[maybe_unused]] const easylocal::pareto_archive<std::string, cost_type>
                empty{{.max_front_size = 0}};
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        ok &= expect(rejected, "a front of no point is rejected");
    }

    {
        // The relations of the search, not the cost's operators: here larger
        // values dominate, as a root compare that maximizes would say.
        struct Maximizing
        {
            [[nodiscard]] static bool better(const cost_type& lhs, const cost_type& rhs)
            {
                return rhs < lhs;
            }

            [[nodiscard]] static bool equivalent(
                const cost_type& lhs,
                const cost_type& rhs)
            {
                return lhs == rhs;
            }
        };
        easylocal::pareto_archive<std::string, cost_type> archive;
        archive.offer("low", low_first, Maximizing{}, same_name);
        ok &= expect(
            archive.offer("high", dominated, Maximizing{}, same_name)
                && archive.size() == 1 && archive.points().front().solution == "high"
                && !archive.offer("low", low_first, Maximizing{}, same_name),
            "the archive compares costs with the relations it is given");
    }

    return ok ? 0 : 1;
}
