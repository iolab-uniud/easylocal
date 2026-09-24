#pragma once

#include <easylocal/runner.hpp>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace easylocal
{

namespace detail
{

template<std::size_t Index, class Move>
struct tagged_neighborhood_move
{
    static constexpr std::size_t index = Index;
    using value_type = Move;

    Move value;
};

template<std::size_t Index, class Iterator>
struct tagged_neighborhood_iterator
{
    static constexpr std::size_t index = Index;
    Iterator current;
};

template<class Sequence, class... Explorers>
struct neighborhood_union_move_type;

template<std::size_t... Indices, class... Explorers>
struct neighborhood_union_move_type<
    std::index_sequence<Indices...>,
    Explorers...>
{
    using type = std::variant<
        tagged_neighborhood_move<Indices, typename Explorers::move_type>...>;
};

template<class Solution, class Move, class... Ranges>
class neighborhood_union_moves_view
    : public std::ranges::view_interface<
          neighborhood_union_moves_view<Solution, Move, Ranges...>>
{
private:
    static constexpr std::size_t range_count = sizeof...(Ranges);
    using ranges_type = std::tuple<Ranges...>;

    template<std::size_t Index>
    using range_type = std::tuple_element_t<Index, ranges_type>;

    template<std::size_t Index>
    using iterator_type = std::ranges::iterator_t<range_type<Index>>;

    template<class Sequence>
    struct iterator_variant_type;

    template<std::size_t... Indices>
    struct iterator_variant_type<std::index_sequence<Indices...>>
    {
        using type = std::variant<
            tagged_neighborhood_iterator<Indices, iterator_type<Indices>>...>;
    };

    using iterator_variant = typename iterator_variant_type<
        std::make_index_sequence<range_count>>::type;

public:
    class iterator
    {
    public:
        using value_type = Move;
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::input_iterator_tag;
        using iterator_category = std::input_iterator_tag;

        iterator() = default;

        [[nodiscard]]
        auto operator*() const -> value_type
        {
            return std::visit(
                []<class TaggedIterator>(const TaggedIterator& tagged) {
                    constexpr auto index = TaggedIterator::index;
                    using child_move_type = std::variant_alternative_t<
                        index,
                        value_type>;
                    using move_type = typename child_move_type::value_type;

                    return value_type{
                        std::in_place_index<index>,
                        child_move_type{
                            static_cast<move_type>(*tagged.current),
                        },
                    };
                },
                *current_);
        }

        auto operator++() -> iterator&
        {
            const bool exhausted = std::visit(
                [this]<class TaggedIterator>(TaggedIterator& tagged) {
                    constexpr auto index = TaggedIterator::index;
                    ++tagged.current;
                    return tagged.current ==
                           std::ranges::end(
                               std::get<index>(parent_->ranges_));
                },
                *current_);

            if (exhausted)
            {
                parent_->seek(*this, range_index_ + 1);
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
            return !current.current_.has_value();
        }

        friend auto operator==(
            std::default_sentinel_t sentinel,
            const iterator& current) noexcept -> bool
        {
            return current == sentinel;
        }

    private:
        friend class neighborhood_union_moves_view;

        explicit iterator(neighborhood_union_moves_view* parent)
            : parent_{parent}
        {
            parent_->seek(*this, 0);
        }

        neighborhood_union_moves_view* parent_{};
        std::size_t range_index_{range_count};
        std::optional<iterator_variant> current_;
    };

    explicit neighborhood_union_moves_view(Ranges... ranges)
        : ranges_{std::forward<Ranges>(ranges)...}
    {
    }

    [[nodiscard]]
    auto begin() -> iterator
    {
        return iterator{this};
    }

    [[nodiscard]]
    static auto end() noexcept -> std::default_sentinel_t
    {
        return {};
    }

private:
    template<std::size_t Index = 0>
    void seek(iterator& target, const std::size_t start)
    {
        if constexpr (Index < range_count)
        {
            if (Index >= start)
            {
                auto first = std::ranges::begin(std::get<Index>(ranges_));
                const auto last = std::ranges::end(std::get<Index>(ranges_));

                if (first != last)
                {
                    target.range_index_ = Index;
                    target.current_.emplace(
                        std::in_place_index<Index>,
                        tagged_neighborhood_iterator<Index, iterator_type<Index>>{
                            std::move(first),
                        });
                    return;
                }
            }

            seek<Index + 1>(target, start);
        }
        else
        {
            target.range_index_ = range_count;
            target.current_.reset();
        }
    }

    ranges_type ranges_;
};

template<class... Explorers>
class neighborhood_union_explorer
{
    static_assert(
        sizeof...(Explorers) >= 2,
        "a neighborhood union requires at least two NeighborhoodExplorers");

private:
    using explorer_tuple = std::tuple<Explorers...>;
    using first_explorer = std::tuple_element_t<0, explorer_tuple>;

    template<std::size_t... Indices>
    [[nodiscard]]
    auto make_moves_view(
        const typename first_explorer::solution_type& solution,
        std::index_sequence<Indices...>) const
    {
        using move_view = neighborhood_union_moves_view<
            typename first_explorer::solution_type,
            typename neighborhood_union_move_type<
                std::index_sequence_for<Explorers...>,
                Explorers...>::type,
            decltype(std::get<Indices>(explorers_).moves(solution))...>;

        return move_view{
            std::get<Indices>(explorers_).moves(solution)...,
        };
    }

public:
    using instance_type = typename first_explorer::instance_type;
    using solution_type = typename first_explorer::solution_type;
    using move_type = typename neighborhood_union_move_type<
        std::index_sequence_for<Explorers...>,
        Explorers...>::type;

    static_assert(
        (std::same_as<
             instance_type,
             typename Explorers::instance_type> && ...),
        "all NeighborhoodExplorers in a union must use the same instance_type");
    static_assert(
        (std::same_as<
             solution_type,
             typename Explorers::solution_type> && ...),
        "all NeighborhoodExplorers in a union must use the same solution_type");

    explicit neighborhood_union_explorer(Explorers... explorers)
        : explorers_{std::move(explorers)...}
    {
#ifndef NDEBUG
        const auto* const expected =
            std::addressof(std::get<0>(explorers_).instance());

        std::apply(
            [expected](const auto&... explorer) {
                assert(
                    ((std::addressof(explorer.instance()) == expected) && ...) &&
                    "all child NeighborhoodExplorers in a union must share the same Instance");
            },
            explorers_);
#endif
    }

    [[nodiscard]]
    auto instance() const noexcept -> const instance_type&
    {
        return std::get<0>(explorers_).instance();
    }

    [[nodiscard]]
    auto moves(const solution_type& solution) const
    {
        return make_moves_view(
            solution,
            std::index_sequence_for<Explorers...>{});
    }

    void make_move(solution_type& solution, const move_type& move) const
    {
        std::visit(
            [this, &solution]<class TaggedMove>(const TaggedMove& tagged) {
                constexpr auto index = TaggedMove::index;
                std::get<index>(explorers_).make_move(solution, tagged.value);
            },
            move);
    }

private:
    explorer_tuple explorers_;
};

template<class... Specs>
class neighborhood_union_spec
{
    static_assert(
        sizeof...(Specs) >= 2,
        "a neighborhood union requires at least two neighborhood specs");
    static_assert(
        (is_neighborhood_spec_v<Specs> && ...),
        "every operand of a neighborhood union must be a neighborhood spec");

public:
    using service_type = neighborhood_union_explorer<service_t<Specs>...>;

    explicit neighborhood_union_spec(Specs... specs)
        : specs_{std::move(specs)...}
    {
    }

    template<class Dependency>
    static constexpr bool constructible_from =
        (Specs::template constructible_from<Dependency> && ...);

    template<class Dependency>
        requires constructible_from<Dependency>
    [[nodiscard]]
    auto construct(Dependency& dependency) const -> service_type
    {
        return construct_impl(
            dependency,
            std::index_sequence_for<Specs...>{});
    }

private:
    template<class Dependency, std::size_t... Indices>
    [[nodiscard]]
    auto construct_impl(
        Dependency& dependency,
        std::index_sequence<Indices...>) const -> service_type
    {
        return service_type{
            std::get<Indices>(specs_).construct(dependency)...,
        };
    }

    std::tuple<Specs...> specs_;
};

template<class... Specs>
struct is_neighborhood_spec<neighborhood_union_spec<Specs...>>
    : std::true_type
{
};

} // namespace detail

template<class... Specs>
    requires (sizeof...(Specs) >= 2) &&
             (detail::is_neighborhood_spec_v<std::remove_cvref_t<Specs>> && ...)
[[nodiscard]]
auto neighborhood_union(Specs&&... specs)
{
    return detail::neighborhood_union_spec<std::remove_cvref_t<Specs>...>{
        std::forward<Specs>(specs)...,
    };
}

} // namespace easylocal
