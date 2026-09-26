#pragma once

#include <easylocal/neighborhood_concepts.hpp>

#include <cassert>
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

} // namespace easylocal
