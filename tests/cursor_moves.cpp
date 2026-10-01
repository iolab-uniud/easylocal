#include <easylocal/helpers/neighborhood_explorer.hpp>

#include <concepts>
#include <cstddef>
#include <iostream>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

struct Solution
{
    std::size_t size;
};

struct Move
{
    std::size_t index{};

    friend auto operator==(const Move&, const Move&) -> bool = default;
};

class CursorExplorer
{
public:
    using solution_type = Solution;
    using move_type = Move;

    [[nodiscard]]
    auto first_move(const Solution& solution, Move& move) const noexcept -> bool
    {
        ++first_calls_;

        if (solution.size == 0)
        {
            return false;
        }

        move.index = 0;
        return true;
    }

    [[nodiscard]]
    auto next_move(const Solution& solution, Move& move) const noexcept -> bool
    {
        ++next_calls_;
        ++move.index;
        return move.index < solution.size;
    }

    [[nodiscard]]
    auto first_calls() const noexcept -> std::size_t
    {
        return first_calls_;
    }

    [[nodiscard]]
    auto next_calls() const noexcept -> std::size_t
    {
        return next_calls_;
    }

private:
    mutable std::size_t first_calls_{};
    mutable std::size_t next_calls_{};
};

struct NonDefaultMove
{
    explicit NonDefaultMove(std::size_t value) noexcept
        : index{value}
    {
    }

    std::size_t index;
};

class NonDefaultMoveExplorer
{
public:
    using solution_type = Solution;
    using move_type = NonDefaultMove;

    [[nodiscard]]
    auto first_move(const Solution&, NonDefaultMove&) const noexcept -> bool
    {
        return false;
    }

    [[nodiscard]]
    auto next_move(const Solution&, NonDefaultMove&) const noexcept -> bool
    {
        return false;
    }
};


class DualProtocolExplorer
{
public:
    using solution_type = Solution;
    using move_type = Move;

    [[nodiscard]] auto first_move(const Solution& solution, Move& move) const noexcept -> bool
    {
        if (solution.size == 0) return false;
        move.index = 7;
        return true;
    }

    [[nodiscard]] auto next_move(const Solution&, Move&) const noexcept -> bool
    {
        return false;
    }

    [[nodiscard]] auto moves(const Solution&) const
    {
        return std::views::single(Move{.index = 99});
    }
};

template<class Explorer>
concept CanAdaptCursor =
    requires(
        const Explorer& explorer,
        const typename Explorer::solution_type& solution)
    {
        easylocal::cursor_moves(explorer, solution);
    };

auto expect(const bool condition, const std::string_view description) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }

    return true;
}

template<std::ranges::input_range Range>
auto collect(Range&& range) -> std::vector<Move>
{
    std::vector<Move> result;

    for (const auto move : range)
    {
        result.push_back(move);
    }

    return result;
}

} // namespace

int main()
{
    bool ok = true;

    static_assert(CanAdaptCursor<CursorExplorer>);
    static_assert(!CanAdaptCursor<NonDefaultMoveExplorer>);
    static_assert(easylocal::detail::has_ambiguous_moves_interface_v<DualProtocolExplorer>);

    const CursorExplorer explorer;
    const Solution solution{.size = 4};

    auto moves = easylocal::cursor_moves(explorer, solution);

    static_assert(std::ranges::input_range<decltype(moves)>);
    static_assert(!std::ranges::forward_range<decltype(moves)>);
    static_assert(std::ranges::view<decltype(moves)>);
    static_assert(std::same_as<
        std::ranges::range_value_t<decltype(moves)>,
        Move>);
    static_assert(std::same_as<
        std::ranges::range_reference_t<decltype(moves)>,
        const Move&>);

    ok &= expect(
        explorer.first_calls() == 0 && explorer.next_calls() == 0,
        "constructing the cursor view does not start traversal");

    const std::vector<Move> expected{
        Move{.index = 0},
        Move{.index = 1},
        Move{.index = 2},
        Move{.index = 3},
    };

    ok &= expect(
        collect(moves) == expected,
        "cursor adapter preserves FirstMove/NextMove order");
    ok &= expect(
        explorer.first_calls() == 1 && explorer.next_calls() == 4,
        "one complete traversal performs one first_move and one next_move per yielded move");

    auto odd_moves =
        easylocal::cursor_moves(explorer, solution)
        | std::views::filter([](const Move move) {
              return move.index % 2 == 1;
          });

    const std::vector<Move> expected_odd{
        Move{.index = 1},
        Move{.index = 3},
    };

    ok &= expect(
        collect(odd_moves) == expected_odd,
        "cursor adapter composes with standard views");

    const DualProtocolExplorer dual;
    const auto dual_moves = easylocal::moves(dual, solution);
    ok &= expect(
        collect(dual_moves) == std::vector<Move>{Move{.index = 7}},
        "unified moves customization prefers the EL3 cursor protocol when both interfaces exist");

    const Solution empty{.size = 0};
    auto empty_moves = easylocal::cursor_moves(explorer, empty);
    ok &= expect(
        empty_moves.begin() == empty_moves.end(),
        "cursor adapter represents an empty neighborhood");

    return ok ? 0 : 1;
}
