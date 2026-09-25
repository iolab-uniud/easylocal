#pragma once

#include <easylocal/config/parameters.hpp>
#include <easylocal/runner.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <random>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace easylocal
{

template<std::size_t Count>
struct NeighborhoodUnionParameters
{
    static_assert(
        Count >= 2,
        "a neighborhood union parameter block requires at least two children");

    std::array<double, Count> random_biases = [] {
        std::array<double, Count> biases{};
        biases.fill(1.0);
        return biases;
    }();

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "random_biases",
                &NeighborhoodUnionParameters::random_biases>(
                    "Relative weights for random child-neighborhood selection"));
    }

    [[nodiscard]]
    auto validate() const noexcept -> config::validation_result
    {
        for (const auto bias : random_biases)
        {
            if (!std::isfinite(bias) || bias < 0.0)
            {
                return config::validation_result::failure(
                    "random_biases must be finite and non-negative");
            }
        }

        return config::validation_result::success();
    }
};

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

template<class Move, class... Ranges>
class neighborhood_union_moves_view
    : public std::ranges::view_interface<
          neighborhood_union_moves_view<Move, Ranges...>>
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

template<class Explorer, class Solution, class RNG>
concept random_move_neighborhood =
    requires(
        const Explorer& explorer,
        const Solution& solution,
        RNG& rng)
    {
        {
            explorer.random_move(solution, rng)
        } -> std::same_as<std::optional<typename Explorer::move_type>>;
    };

template<class Bindings>
struct delta_binding_component_types;

template<class... Bindings>
struct delta_binding_component_types<std::tuple<Bindings...>>
{
    using type = std::tuple<typename Bindings::component_type...>;
};

template<class Explorer>
using neighborhood_delta_component_types_t = typename delta_binding_component_types<
    neighborhood_delta_bindings_t<Explorer>>::type;

template<class Explorer, class Component>
inline constexpr bool neighborhood_has_delta_component_v =
    tuple_contains_type_v<
        Component,
        neighborhood_delta_component_types_t<Explorer>>;

template<class Component, class... Explorers>
inline constexpr bool all_neighborhoods_have_delta_component_v =
    (neighborhood_has_delta_component_v<Explorers, Component> && ...);

template<class CandidateComponents, class... Explorers>
struct common_neighborhood_delta_components;

template<class... Components, class... Explorers>
struct common_neighborhood_delta_components<
    std::tuple<Components...>,
    Explorers...>
{
    using type = decltype(std::tuple_cat(
        std::declval<std::conditional_t<
            all_neighborhoods_have_delta_component_v<
                Components,
                Explorers...>,
            std::tuple<Components>,
            std::tuple<>>>()...));
};

template<class Component, class... Explorers>
class neighborhood_union_delta_binding
{
private:
    using explorers_type = std::tuple<Explorers...>;

    template<std::size_t Index>
    using explorer_type = std::tuple_element_t<Index, explorers_type>;

public:
    using component_type = Component;

    explicit neighborhood_union_delta_binding(
        const explorers_type& explorers) noexcept
        : explorers_{std::addressof(explorers)}
    {
    }

    template<class Value, class Solution, class UnionMove>
    [[nodiscard]]
    auto apply(
        const Value& value,
        const Solution& solution,
        const UnionMove& move) const -> Value
    {
        return std::visit(
            [this, &value, &solution]<class TaggedMove>(
                const TaggedMove& tagged) -> Value {
                constexpr auto child_index = TaggedMove::index;
                using child_explorer_type = explorer_type<child_index>;
                using child_component_types =
                    neighborhood_delta_component_types_t<child_explorer_type>;
                constexpr auto binding_index = tuple_type_index_v<
                    Component,
                    child_component_types>;

                const auto& child = std::get<child_index>(*explorers_);
                auto&& child_bindings = child.delta_bindings();

                return std::get<binding_index>(child_bindings)
                    .apply(value, solution, tagged.value);
            },
            move);
    }

private:
    const explorers_type* explorers_;
};

template<class Components, class... Explorers>
struct neighborhood_union_delta_bindings;

template<class... Components, class... Explorers>
struct neighborhood_union_delta_bindings<
    std::tuple<Components...>,
    Explorers...>
{
    using type = std::tuple<neighborhood_union_delta_binding<
        Components,
        Explorers...>...>;
};

template<std::size_t Count>
[[nodiscard]]
auto valid_random_biases(const std::array<double, Count>& biases) noexcept
    -> bool
{
    for (const auto bias : biases)
    {
        if (!std::isfinite(bias) || bias < 0.0)
        {
            return false;
        }
    }

    return true;
}

template<class... Explorers>
class neighborhood_union_explorer
{
    static_assert(
        sizeof...(Explorers) >= 2,
        "a neighborhood union requires at least two NeighborhoodExplorers");

private:
    static constexpr std::size_t child_count = sizeof...(Explorers);

    using explorer_tuple = std::tuple<Explorers...>;
    using first_explorer = std::tuple_element_t<0, explorer_tuple>;
    using union_move_type = typename neighborhood_union_move_type<
        std::index_sequence_for<Explorers...>,
        Explorers...>::type;
    using common_delta_components = typename common_neighborhood_delta_components<
        neighborhood_delta_component_types_t<first_explorer>,
        Explorers...>::type;

    template<std::size_t... Indices>
    [[nodiscard]]
    auto make_moves_view(
        const typename first_explorer::solution_type& solution,
        std::index_sequence<Indices...>) const
    {
        using move_view = neighborhood_union_moves_view<
            union_move_type,
            decltype(std::get<Indices>(explorers_).moves(solution))...>;

        return move_view{
            std::get<Indices>(explorers_).moves(solution)...,
        };
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto choose_random_child(
        const std::array<bool, child_count>& active,
        RNG& rng) const -> std::optional<std::size_t>
    {
        double max_bias = 0.0;

        for (std::size_t index = 0; index < active.size(); ++index)
        {
            if (active[index])
            {
                max_bias = std::max(max_bias, random_biases_[index]);
            }
        }

        if (max_bias == 0.0)
        {
            return std::nullopt;
        }

        double total = 0.0;
        for (std::size_t index = 0; index < active.size(); ++index)
        {
            if (active[index])
            {
                total += random_biases_[index] / max_bias;
            }
        }

        std::uniform_real_distribution<double> draw{0.0, total};
        const auto target = draw(rng);
        double cumulative = 0.0;
        std::size_t fallback = active.size();

        for (std::size_t index = 0; index < active.size(); ++index)
        {
            if (!active[index])
            {
                continue;
            }

            fallback = index;
            cumulative += random_biases_[index] / max_bias;
            if (target < cumulative)
            {
                return index;
            }
        }

        assert(fallback != active.size());
        return fallback;
    }

    template<std::size_t Index = 0, std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto random_move_from_child(
        const std::size_t selected,
        const typename first_explorer::solution_type& solution,
        RNG& rng) const
        -> std::optional<union_move_type>
    {
        if constexpr (Index < child_count)
        {
            if (selected == Index)
            {
                auto child_move =
                    std::get<Index>(explorers_).random_move(solution, rng);

                if (!child_move)
                {
                    return std::nullopt;
                }

                using tagged_move_type =
                    std::variant_alternative_t<Index, union_move_type>;

                return union_move_type{
                    std::in_place_index<Index>,
                    tagged_move_type{std::move(*child_move)},
                };
            }

            return random_move_from_child<Index + 1>(
                selected,
                solution,
                rng);
        }
        else
        {
            assert(false && "selected neighborhood index must be valid");
            return std::nullopt;
        }
    }

public:
    using instance_type = typename first_explorer::instance_type;
    using solution_type = typename first_explorer::solution_type;
    using move_type = union_move_type;
    using delta_bindings_type = typename neighborhood_union_delta_bindings<
        common_delta_components,
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

    explicit neighborhood_union_explorer(
        std::array<double, child_count> random_biases,
        Explorers... explorers)
        : explorers_{std::move(explorers)...},
          random_biases_{std::move(random_biases)}
    {
        assert(
            valid_random_biases(random_biases_) &&
            "neighborhood random biases must be finite and non-negative");
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

    [[nodiscard]]
    auto delta_bindings() const noexcept -> delta_bindings_type
    {
        return make_delta_bindings(
            std::type_identity<common_delta_components>{});
    }

    template<std::uniform_random_bit_generator RNG>
        requires (random_move_neighborhood<
                      Explorers,
                      solution_type,
                      RNG> && ...)
    [[nodiscard]]
    auto random_move(
        const solution_type& solution,
        RNG& rng) const -> std::optional<move_type>
    {
        std::array<bool, child_count> active{};
        for (std::size_t index = 0; index < active.size(); ++index)
        {
            active[index] = random_biases_[index] > 0.0;
        }

        while (true)
        {
            const auto selected = choose_random_child(active, rng);
            if (!selected)
            {
                return std::nullopt;
            }

            if (auto move = random_move_from_child(*selected, solution, rng))
            {
                return move;
            }

            active[*selected] = false;
        }
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
    template<class... Components>
    [[nodiscard]]
    auto make_delta_bindings(
        std::type_identity<std::tuple<Components...>>) const noexcept
        -> delta_bindings_type
    {
        return delta_bindings_type{
            neighborhood_union_delta_binding<
                Components,
                Explorers...>{explorers_}...,
        };
    }

    explorer_tuple explorers_;
    std::array<double, child_count> random_biases_;
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
        random_biases_.fill(1.0);
    }

    [[nodiscard]]
    auto with_random_biases(
        std::array<double, sizeof...(Specs)> random_biases) &&
        -> neighborhood_union_spec
    {
        assert(
            valid_random_biases(random_biases) &&
            "neighborhood random biases must be finite and non-negative");
        random_biases_ = std::move(random_biases);
        return std::move(*this);
    }

    template<class Dependency>
    static constexpr bool constructible_from =
        (Specs::template constructible_from<Dependency> && ...);

    template<class Dependency>
        requires constructible_from<Dependency>
    [[nodiscard]]
    auto construct(Dependency& dependency) const -> service_type
    {
        static_assert(
            (validate_delta_bindings<
                 std::remove_cvref_t<Dependency>,
                 service_t<Specs>>() && ...),
            "every child neighborhood in a union must have valid delta bindings");

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
            random_biases_,
            std::get<Indices>(specs_).construct(dependency)...,
        };
    }

    std::tuple<Specs...> specs_;
    std::array<double, sizeof...(Specs)> random_biases_{};
};

template<class... Specs>
struct is_neighborhood_spec<neighborhood_union_spec<Specs...>>
    : std::true_type
{
};

template<std::size_t Count>
struct random_biases_spec
{
    std::array<double, Count> values;
};

} // namespace detail

template<std::convertible_to<double>... Weights>
    requires (sizeof...(Weights) >= 2)
[[nodiscard]]
auto random_biases(Weights&&... weights)
{
    return detail::random_biases_spec<sizeof...(Weights)>{
        .values = {static_cast<double>(std::forward<Weights>(weights))...},
    };
}

template<class... Specs, std::size_t Count>
    requires (sizeof...(Specs) == Count)
[[nodiscard]]
auto operator|(
    detail::neighborhood_union_spec<Specs...> spec,
    detail::random_biases_spec<Count> biases)
{
    return std::move(spec).with_random_biases(std::move(biases.values));
}

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
