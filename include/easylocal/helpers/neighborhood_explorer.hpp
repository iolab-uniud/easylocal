#pragma once

/// \file
/// NeighborhoodExplorer: which moves exist, whether one is valid and how it
/// changes a solution, with the functions the algorithms enumerate and draw
/// moves through.

#include <easylocal/utils/detail/meta.hpp>
#include <easylocal/utils/generator.hpp> // IWYU pragma: export
#include <easylocal/utils/hash.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <type_traits>
#include <utility>

namespace easylocal
{

namespace detail
{

template<class Result, class Move>
concept move_optional_for_impl =
    requires { typename optional_value_t<Result>; } &&
    std::constructible_from<Move, const optional_value_t<Result>&>;

// Whether make_move would take a temporary Solution: by value or by const
// reference, it changes a copy, and the search never moves.
template<class NHE>
consteval bool make_move_takes_a_copy()
{
    if constexpr (requires {
                      typename NHE::solution_type;
                      typename NHE::move_type;
                  })
    {
        return requires(
            const NHE& neighborhood,
            typename NHE::solution_type&& solution,
            const typename NHE::move_type& move) {
            neighborhood.make_move(std::move(solution), move);
        };
    }
    else
        return false;
}

} // namespace detail

/// An input range whose elements construct a `Move`.
template<class Range, class Move>
concept move_input_range_for =
    std::ranges::input_range<Range> &&
    std::constructible_from<Move, std::ranges::range_reference_t<Range>>;

/// A `std::optional` whose value constructs a `Move`.
template<class Result, class Move>
concept move_optional_for = detail::move_optional_for_impl<Result, Move>;

/// A NeighborhoodExplorer for the SolutionManager `SM`: it declares `move_type`
/// and has `is_valid(solution, move)` and `make_move(solution, move)`.
template<class NHE, class SM>
concept neighborhood_explorer_for =
    requires(
        const NHE& neighborhood,
        const typename SM::solution_type& solution,
        typename SM::solution_type& mutable_solution,
        const typename NHE::move_type& move)
    {
        typename SM::input_type;
        typename SM::solution_type;
        typename NHE::move_type;

        {
            neighborhood.is_valid(solution, move)
        } -> std::convertible_to<bool>;

        {
            neighborhood.make_move(mutable_solution, move)
        } -> std::same_as<void>;
    };

/// An explorer that enumerates its moves with the cursor of EasyLocal 3.
///
/// `first_move(solution, move)` and `next_move(solution, move)` set `move` and
/// return whether there was one; its `move_type` is default-initializable.
template<class NHE, class Solution>
concept cursor_neighborhood_for =
    requires(
        const NHE& neighborhood,
        const Solution& solution,
        typename NHE::move_type& move)
    {
        typename NHE::move_type;
        requires std::default_initializable<typename NHE::move_type>;

        {
            neighborhood.first_move(solution, move)
        } -> std::same_as<bool>;

        {
            neighborhood.next_move(solution, move)
        } -> std::same_as<bool>;
    };

/// An explorer whose `moves(solution)` returns an input range of moves, such as
/// a generator.
template<class NHE, class Solution>
concept native_moves_neighborhood_for =
    requires(const NHE& neighborhood, const Solution& solution)
    {
        typename NHE::move_type;
        requires move_input_range_for<
            decltype(neighborhood.moves(solution)),
            typename NHE::move_type>;
    };

/// An explorer that enumerates its moves, with the cursor or with `moves()`.
template<class NHE, class Solution>
concept deterministic_neighborhood_for =
    cursor_neighborhood_for<NHE, Solution> ||
    native_moves_neighborhood_for<NHE, Solution>;

/// An explorer that draws moves: `random_move(solution, rng)` returns a
/// `std::optional` whose value constructs a move, empty when it has none.
template<class NHE, class Solution, class RNG>
concept random_neighborhood_for =
    requires(
        const NHE& neighborhood,
        const Solution& solution,
        RNG& rng)
    {
        typename NHE::move_type;
        requires move_optional_for<
            decltype(neighborhood.random_move(solution, rng)),
            typename NHE::move_type>;
    };

namespace detail
{

#if defined(NDEBUG) && (defined(__GNUC__) || defined(__clang__))
#define EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE inline __attribute__((always_inline))
#else
#define EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE inline
#endif

template<class Explorer, class Solution>
    requires cursor_neighborhood_for<Explorer, Solution>
class cursor_moves_view
    : public std::ranges::view_interface<cursor_moves_view<Explorer, Solution>>
{
public:
    using solution_type = Solution;
    using move_type = typename Explorer::move_type;

    cursor_moves_view() = default;

    EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE
    cursor_moves_view(
        const Explorer& explorer,
        const solution_type& solution) noexcept
        : explorer_{std::addressof(explorer)},
          solution_{std::addressof(solution)}
    {
    }

    class iterator
    {
    public:
        using iterator_concept = std::input_iterator_tag;
        using value_type = move_type;
        using difference_type = std::ptrdiff_t;

        iterator() = default;

        EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE
        iterator(
            const Explorer& explorer,
            const solution_type& solution)
            noexcept(noexcept(
                std::declval<const Explorer&>().first_move(
                    std::declval<const solution_type&>(),
                    std::declval<move_type&>())))
            : explorer_{std::addressof(explorer)},
              solution_{std::addressof(solution)}
        {
            if (!explorer_->first_move(*solution_, current_)) [[unlikely]]
            {
                explorer_ = nullptr;
            }
        }

        [[nodiscard]]
        EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE const move_type& operator*() const noexcept
        {
            assert(explorer_ != nullptr);
            return current_;
        }

        EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE
        iterator& operator++() noexcept(
            noexcept(std::declval<const Explorer&>().next_move(
                std::declval<const solution_type&>(),
                std::declval<move_type&>())))
        {
            assert(explorer_ != nullptr);

            const auto* const explorer = explorer_;
            if (!explorer->next_move(*solution_, current_)) [[unlikely]]
            {
                explorer_ = nullptr;
            }

            return *this;
        }

        EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE
        void operator++(int)
            noexcept(noexcept(++std::declval<iterator&>()))
        {
            ++*this;
        }

        friend EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE bool operator==(
            const iterator& current,
            std::default_sentinel_t) noexcept
        {
            return current.explorer_ == nullptr;
        }

    private:
        const Explorer* explorer_{nullptr};
        const solution_type* solution_{nullptr};
        move_type current_{};
    };

    [[nodiscard]]
    EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE iterator begin() const
        noexcept(noexcept(iterator{*explorer_, *solution_}))
    {
        assert(explorer_ != nullptr);
        assert(solution_ != nullptr);
        return iterator{*explorer_, *solution_};
    }

    [[nodiscard]]
    EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE std::default_sentinel_t end() const noexcept
    {
        return {};
    }

private:
    const Explorer* explorer_{nullptr};
    const solution_type* solution_{nullptr};
};

#undef EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE

} // namespace detail

/// The moves of an explorer with the cursor of EasyLocal 3, as a lazy input
/// range.
template<class Explorer, class Solution>
    requires cursor_neighborhood_for<Explorer, Solution>
[[nodiscard]]
inline detail::cursor_moves_view<Explorer, Solution> cursor_moves(
    const Explorer& explorer,
    const Solution& solution) noexcept
{
    return detail::cursor_moves_view<Explorer, Solution>{explorer, solution};
}

/// The moves of an explorer at solution, as an input range: its cursor, when it
/// has one, else its moves().
///
/// The cursor wins when the explorer has both. A reference that the explorer's
/// moves() returns, to moves it keeps, stays a reference: they are not copied.
template<class Explorer, class Solution>
    requires deterministic_neighborhood_for<Explorer, Solution>
[[nodiscard]]
inline decltype(auto) moves(const Explorer& explorer, const Solution& solution)
{
    if constexpr (cursor_neighborhood_for<Explorer, Solution>)
    {
        return cursor_moves(explorer, solution);
    }
    else
    {
        return explorer.moves(solution);
    }
}

/// A move of an explorer drawn at solution with rng, by its random_move();
/// empty when it has none.
///
/// The explorer may return `std::optional<T>` for any T from which its
/// move_type can be built.
template<class Explorer, class Solution, std::uniform_random_bit_generator RNG>
    requires random_neighborhood_for<Explorer, Solution, RNG>
[[nodiscard]]
inline std::optional<typename Explorer::move_type> random_move(
    const Explorer& explorer,
    const Solution& solution,
    RNG& rng)
{
    auto result = explorer.random_move(solution, rng);
    if (!result)
    {
        return std::nullopt;
    }

    return std::optional<typename Explorer::move_type>(std::in_place, std::move(*result));
}

/// An explorer with the inverse(solution, move, tabu_move) Tabu Search needs.
///
/// inverse(solution, move, tabu_move) says whether move, proposed at solution,
/// is forbidden by tabu_move, a move applied earlier, typically because it
/// would undo it. There is no default: what forbids what (the same pair of
/// jobs, or any move of either job) is a modelling choice of the neighborhood,
/// and may be one of its parameters.
template<class NHE, class Solution>
concept inverse_neighborhood_for = requires(
    const NHE& neighborhood,
    const Solution& solution,
    const typename NHE::move_type& move,
    const typename NHE::move_type& tabu_move) {
    { neighborhood.inverse(solution, move, tabu_move) } -> std::convertible_to<bool>;
};

/// Whether `move`, proposed at `solution`, is forbidden by `tabu_move`, applied
/// earlier, as the explorer's `inverse` decides.
template<class Explorer, class Solution>
    requires inverse_neighborhood_for<Explorer, Solution>
[[nodiscard]]
inline bool inverse(
    const Explorer& explorer,
    const Solution& solution,
    const typename Explorer::move_type& move,
    const typename Explorer::move_type& tabu_move)
{
    return static_cast<bool>(explorer.inverse(solution, move, tabu_move));
}

/// An explorer with a tabu_attribute(move) member, the attribute of a move that
/// frequency-based memory counts: a value with std::hash and ==.
///
/// The member chooses it (the pair of jobs of a swap, ignoring
/// their positions); without one, the move itself is the attribute when it has
/// std::hash and ==.
template<class NHE>
concept has_tabu_attribute_member =
    requires(const NHE& neighborhood, const typename NHE::move_type& move) {
        requires std_hashable<
            std::remove_cvref_t<decltype(neighborhood.tabu_attribute(move))>>;
        requires std::equality_comparable<
            std::remove_cvref_t<decltype(neighborhood.tabu_attribute(move))>>;
    };

/// An explorer whose moves have a tabu attribute: its `tabu_attribute(move)`
/// member, or the move itself when it has `std::hash` and `==`.
template<class NHE>
concept has_tabu_attribute = has_tabu_attribute_member<NHE>
    || (std_hashable<typename NHE::move_type>
        && std::equality_comparable<typename NHE::move_type>);

/// The attribute of `move` that frequency-based memory counts: the explorer's
/// `tabu_attribute(move)`, or else a copy of the move.
template<has_tabu_attribute Explorer>
[[nodiscard]]
inline auto tabu_attribute(
    const Explorer& explorer,
    const typename Explorer::move_type& move)
{
    if constexpr (has_tabu_attribute_member<Explorer>)
    {
        return std::remove_cvref_t<decltype(explorer.tabu_attribute(move))>{
            explorer.tabu_attribute(move)};
    }
    else
    {
        return typename Explorer::move_type{move};
    }
}

/// The type of the tabu attribute of the moves of `Explorer`.
template<has_tabu_attribute Explorer>
using tabu_attribute_t = decltype(easylocal::tabu_attribute(
    std::declval<const Explorer&>(),
    std::declval<const typename Explorer::move_type&>()));

/// A base for an explorer, which gives it the associated types, the
/// SolutionManager it is built from and input().
///
/// It is optional and non-virtual: the concepts above do not require it.
template<class SolutionManager, class Move>
class neighborhood_explorer_base
{
public:
    /// The SolutionManager type.
    using solution_manager_type = SolutionManager;
    /// The Input type of the SolutionManager.
    using input_type = typename solution_manager_type::input_type;
    /// The Solution type of the SolutionManager.
    using solution_type = typename solution_manager_type::solution_type;
    /// The Move type.
    using move_type = Move;

    /// From the SolutionManager, which it keeps by reference.
    explicit neighborhood_explorer_base(
        const solution_manager_type& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    /// Not from a temporary SolutionManager, which would dangle: the
    /// SolutionManager must outlive the explorer.
    explicit neighborhood_explorer_base(const solution_manager_type&&) = delete;

    /// The Input of the SolutionManager.
    [[nodiscard]]
    const input_type& input() const noexcept
    {
        return solution_manager_.input();
    }

protected:
    /// The SolutionManager the explorer was constructed from.
    const solution_manager_type& solution_manager_;
};

} // namespace easylocal
