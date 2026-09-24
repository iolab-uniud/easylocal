#pragma once

#include <cstddef>
#include <iterator>
#include <ranges>

namespace easylocal::spike::neighborhood_authoring
{

// Adapter for the familiar EL3-style protocol:
//
//   bool first_move(const Solution&, Move&) const;
//   bool next_move (const Solution&, Move&) const;
//
// The adapter is deliberately an input range. It does not impose indexing,
// allocation, or materialization on the problem author.
template<class Explorer>
class cursor_view : public std::ranges::view_interface<cursor_view<Explorer>>
{
public:
    using solution_type = typename Explorer::solution_type;
    using move_type = typename Explorer::move_type;

    cursor_view() = default;

    cursor_view(
        const Explorer& explorer,
        const solution_type& solution) noexcept
        : explorer_{&explorer},
          solution_{&solution}
    {
    }

    class iterator
    {
    public:
        using iterator_concept = std::input_iterator_tag;
        using value_type = move_type;
        using difference_type = std::ptrdiff_t;

        iterator() = default;

        iterator(
            const Explorer& explorer,
            const solution_type& solution)
            : explorer_{&explorer},
              solution_{&solution}
        {
            at_end_ = !explorer_->first_move(*solution_, current_);
        }

        [[nodiscard]]
        auto operator*() const noexcept -> const move_type&
        {
            return current_;
        }

        auto operator++() -> iterator&
        {
            if (!at_end_)
            {
                at_end_ = !explorer_->next_move(*solution_, current_);
            }
            return *this;
        }

        void operator++(int)
        {
            ++*this;
        }

        friend auto operator==(
            const iterator& current,
            std::default_sentinel_t) noexcept -> bool
        {
            return current.at_end_;
        }

    private:
        const Explorer* explorer_{nullptr};
        const solution_type* solution_{nullptr};
        move_type current_{};
        bool at_end_{true};
    };

    [[nodiscard]]
    auto begin() const -> iterator
    {
        return iterator{*explorer_, *solution_};
    }

    [[nodiscard]]
    auto end() const noexcept -> std::default_sentinel_t
    {
        return {};
    }

private:
    const Explorer* explorer_{nullptr};
    const solution_type* solution_{nullptr};
};

template<class Explorer>
[[nodiscard]]
auto cursor_moves(
    const Explorer& explorer,
    const typename Explorer::solution_type& solution) noexcept
    -> cursor_view<Explorer>
{
    return cursor_view<Explorer>{explorer, solution};
}

} // namespace easylocal::spike::neighborhood_authoring
