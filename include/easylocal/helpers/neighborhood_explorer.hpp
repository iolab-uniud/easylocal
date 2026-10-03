#pragma once

#include <easylocal/utils/detail/meta.hpp>
#include <easylocal/utils/generator.hpp> // IWYU pragma: export

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

// NeighborhoodExplorer: move validity/application semantics, deterministic
// and random neighborhood protocols and their framework customization points.
namespace easylocal
{

namespace detail
{

template<class Result, class Move>
concept move_optional_for_impl =
    requires { typename optional_value_t<Result>; } &&
    std::constructible_from<Move, const optional_value_t<Result>&>;

} // namespace detail

template<class Range, class Move>
concept move_input_range_for =
    std::ranges::input_range<Range> &&
    std::constructible_from<Move, std::ranges::range_reference_t<Range>>;

template<class Result, class Move>
concept move_optional_for = detail::move_optional_for_impl<Result, Move>;

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

template<class NHE, class Solution>
concept native_moves_neighborhood_for =
    requires(const NHE& neighborhood, const Solution& solution)
    {
        typename NHE::move_type;
        requires move_input_range_for<
            decltype(neighborhood.moves(solution)),
            typename NHE::move_type>;
    };

template<class NHE, class Solution>
concept deterministic_neighborhood_for =
    cursor_neighborhood_for<NHE, Solution> ||
    native_moves_neighborhood_for<NHE, Solution>;

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

template<class Explorer>
inline constexpr bool has_ambiguous_moves_interface_v =
    cursor_neighborhood_for<Explorer, typename Explorer::solution_type> &&
    native_moves_neighborhood_for<Explorer, typename Explorer::solution_type>;

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
        EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE
        auto operator*() const noexcept -> const move_type&
        {
            assert(explorer_ != nullptr);
            return current_;
        }

        EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE
        auto operator++()
            noexcept(noexcept(
                std::declval<const Explorer&>().next_move(
                    std::declval<const solution_type&>(),
                    std::declval<move_type&>()))) -> iterator&
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

        friend EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE
        auto operator==(
            const iterator& current,
            std::default_sentinel_t) noexcept -> bool
        {
            return current.explorer_ == nullptr;
        }

    private:
        const Explorer* explorer_{nullptr};
        const solution_type* solution_{nullptr};
        move_type current_{};
    };

    [[nodiscard]]
    EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE
    auto begin() const
        noexcept(noexcept(iterator{*explorer_, *solution_})) -> iterator
    {
        assert(explorer_ != nullptr);
        assert(solution_ != nullptr);
        return iterator{*explorer_, *solution_};
    }

    [[nodiscard]]
    EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE
    auto end() const noexcept -> std::default_sentinel_t
    {
        return {};
    }

private:
    const Explorer* explorer_{nullptr};
    const solution_type* solution_{nullptr};
};

#undef EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE

} // namespace detail

// Adapt an EL3-style deterministic cursor to the lazy input-range protocol.
template<class Explorer, class Solution>
    requires cursor_neighborhood_for<Explorer, Solution>
[[nodiscard]]
inline auto cursor_moves(
    const Explorer& explorer,
    const Solution& solution) noexcept
    -> detail::cursor_moves_view<Explorer, Solution>
{
    return detail::cursor_moves_view<Explorer, Solution>{explorer, solution};
}

// Unified deterministic-neighborhood customization point. The EL3 cursor
// protocol wins when both cursor and native range protocols are present.
template<class Explorer, class Solution>
    requires deterministic_neighborhood_for<Explorer, Solution>
[[nodiscard]]
inline auto moves(
    const Explorer& explorer,
    const Solution& solution)
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

// Unified random-neighborhood customization point. A neighborhood may return
// std::optional<T> for any T from which its declared move_type can be built.
template<class Explorer, class Solution, std::uniform_random_bit_generator RNG>
    requires random_neighborhood_for<Explorer, Solution, RNG>
[[nodiscard]]
inline auto random_move(
    const Explorer& explorer,
    const Solution& solution,
    RNG& rng) -> std::optional<typename Explorer::move_type>
{
    auto result = explorer.random_move(solution, rng);
    if (!result)
    {
        return std::nullopt;
    }

    return typename Explorer::move_type{*result};
}

// Optional non-virtual convenience base: associated types and the
// SolutionManager reference. Not required by the structural concepts above.
template<class SolutionManager, class Move>
class neighborhood_explorer_base
{
public:
    using solution_manager_type = SolutionManager;
    using input_type = typename solution_manager_type::input_type;
    using solution_type = typename solution_manager_type::solution_type;
    using move_type = Move;

    explicit neighborhood_explorer_base(
        const solution_manager_type& solution_manager) noexcept
        : solution_manager_{solution_manager}
    {
    }

    [[nodiscard]]
    auto input() const noexcept -> const input_type&
    {
        return solution_manager_.input();
    }

protected:
    const solution_manager_type& solution_manager_;
};

} // namespace easylocal
