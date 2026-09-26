#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <ranges>
#include <utility>

namespace easylocal
{

namespace detail
{

template<class Explorer>
concept cursor_neighborhood =
    requires(
        const Explorer& explorer,
        const typename Explorer::solution_type& solution,
        typename Explorer::move_type& move)
    {
        typename Explorer::solution_type;
        typename Explorer::move_type;

        requires std::default_initializable<typename Explorer::move_type>;

        {
            explorer.first_move(solution, move)
        } -> std::same_as<bool>;

        {
            explorer.next_move(solution, move)
        } -> std::same_as<bool>;
    };


template<class Explorer>
concept native_moves_neighborhood =
    requires(
        const Explorer& explorer,
        const typename Explorer::solution_type& solution)
    {
        typename Explorer::solution_type;
        typename Explorer::move_type;
        {
            explorer.moves(solution)
        } -> std::ranges::input_range;
        requires std::same_as<
            std::ranges::range_value_t<decltype(explorer.moves(solution))>,
            typename Explorer::move_type>;
    };

template<class Explorer>
inline constexpr bool has_ambiguous_moves_interface_v =
    cursor_neighborhood<Explorer> && native_moves_neighborhood<Explorer>;

#if defined(NDEBUG) && (defined(__GNUC__) || defined(__clang__))
#define EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE inline __attribute__((always_inline))
#else
#define EASYLOCAL_DETAIL_CURSOR_FORCE_INLINE inline
#endif

template<cursor_neighborhood Explorer>
class cursor_moves_view
    : public std::ranges::view_interface<cursor_moves_view<Explorer>>
{
public:
    using solution_type = typename Explorer::solution_type;
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

// Adapt an EL3-style deterministic cursor
//
//   bool first_move(const Solution&, Move&) const;
//   bool next_move (const Solution&, Move&) const;
//
// to the lazy input-range protocol consumed by EasyLocal++ search algorithms.
// The returned view is non-owning: the explorer and solution must outlive it.
template<detail::cursor_neighborhood Explorer>
[[nodiscard]]
inline auto cursor_moves(
    const Explorer& explorer,
    const typename Explorer::solution_type& solution) noexcept
    -> detail::cursor_moves_view<Explorer>
{
    return detail::cursor_moves_view<Explorer>{explorer, solution};
}


// Unified neighborhood-enumeration customization point. EL3-style cursor
// neighborhoods are preferred when both protocols are present; otherwise a
// native moves(solution) input range is used directly.
template<class Explorer>
    requires detail::cursor_neighborhood<Explorer> ||
             detail::native_moves_neighborhood<Explorer>
[[nodiscard]]
inline auto moves(
    const Explorer& explorer,
    const typename Explorer::solution_type& solution)
{
    if constexpr (detail::cursor_neighborhood<Explorer>)
    {
        return cursor_moves(explorer, solution);
    }
    else
    {
        return explorer.moves(solution);
    }
}

} // namespace easylocal
