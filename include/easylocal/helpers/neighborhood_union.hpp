#pragma once

/// \file
/// neighborhood_union: several NeighborhoodExplorers on the same Solution
/// combined into one, whose move is a std::variant of theirs.
///
/// Enumeration visits every child in turn; random moves pick a child by its
/// configurable bias. The deltas, inverses and attributes of the children are
/// forwarded per child.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/helpers/detail/evaluation.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/recipes.hpp>
#include <easylocal/trace/events.hpp>
#include <easylocal/trace/tracer.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <string>
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
    config::validation_result validate() const noexcept
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

    // Equal when the underlying moves are; absent when Move has no ==.
    bool operator==(const tagged_neighborhood_move&) const = default;
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
        value_type operator*() const
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
                            move_type{*tagged.current},
                        },
                    };
                },
                *current_);
        }

        iterator& operator++()
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

        friend bool operator==(const iterator& current, std::default_sentinel_t) noexcept
        {
            return !current.current_.has_value();
        }

        friend bool operator==(
            std::default_sentinel_t sentinel,
            const iterator& current) noexcept
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
    iterator begin()
    {
        return iterator{this};
    }

    [[nodiscard]]
    static std::default_sentinel_t end() noexcept
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
    Value apply(const Value& value, const Solution& solution, const UnionMove& move) const
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
bool valid_random_biases(const std::array<double, Count>& biases) noexcept
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

template<class Explorer, class = void>
struct logical_neighborhood_type
{
    using type = Explorer;
};

template<class Explorer>
struct logical_neighborhood_type<
    Explorer,
    std::void_t<typename Explorer::base_type>>
{
    using type = std::conditional_t<
        std::derived_from<Explorer, typename Explorer::base_type>,
        typename Explorer::base_type,
        Explorer>;
};

template<class Explorer>
using logical_neighborhood_type_t =
    typename logical_neighborhood_type<Explorer>::type;

template<class... Explorers>
class neighborhood_union_explorer
{
    static_assert(
        sizeof...(Explorers) >= 2,
        "a neighborhood union requires at least two NeighborhoodExplorers");
    static_assert(
        unique_types_v<logical_neighborhood_type_t<Explorers>...>,
        "a neighborhood union cannot contain the same NeighborhoodExplorer "
        "type more than once");

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
            decltype(easylocal::moves(std::get<Indices>(explorers_), solution))...>;

        return move_view{
            easylocal::moves(std::get<Indices>(explorers_), solution)...,
        };
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    std::optional<std::size_t> choose_random_child(
        const std::array<bool, child_count>& active,
        RNG& rng) const
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

    [[nodiscard]]
    double active_bias_total(const std::array<bool, child_count>& active) const noexcept
    {
        double total = 0.0;
        for (std::size_t index = 0; index < active.size(); ++index)
        {
            if (active[index])
            {
                total += random_biases_[index];
            }
        }
        return total;
    }

    template<std::size_t Index = 0, std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    std::optional<union_move_type> random_move_from_child(
        const std::size_t selected,
        const typename first_explorer::solution_type& solution,
        RNG& rng) const
    {
        if constexpr (Index < child_count)
        {
            if (selected == Index)
            {
                auto child_move = easylocal::random_move(
                    std::get<Index>(explorers_), solution, rng);

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

    template<std::size_t Index = 0, std::uniform_random_bit_generator RNG, class Observer>
    [[nodiscard]]
    std::optional<union_move_type> random_move_from_child_traced(
        const std::size_t selected,
        const typename first_explorer::solution_type& solution,
        RNG& rng,
        Observer& observer,
        const trace::neighborhood_route_node* route) const
    {
        if constexpr (Index < child_count)
        {
            if (selected == Index)
            {
                const auto& child = std::get<Index>(explorers_);
                if constexpr (requires {
                    child.random_move_traced(solution, rng, observer, route);
                })
                {
                    auto child_move = child.random_move_traced(
                        solution, rng, observer, route);
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
                else
                {
                    auto child_move = easylocal::random_move(
                        child, solution, rng);
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
            }

            return random_move_from_child_traced<Index + 1>(
                selected, solution, rng, observer, route);
        }
        else
        {
            assert(false && "selected neighborhood index must be valid");
            return std::nullopt;
        }
    }

public:
    using input_type = typename first_explorer::input_type;
    using solution_type = typename first_explorer::solution_type;
    using move_type = union_move_type;
    using delta_bindings_type = typename neighborhood_union_delta_bindings<
        common_delta_components,
        Explorers...>::type;

    static_assert(
        (std::same_as<
             input_type,
             typename Explorers::input_type> && ...),
        "all NeighborhoodExplorers in a union must use the same input_type");
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
            std::addressof(std::get<0>(explorers_).input());

        std::apply(
            [expected](const auto&... explorer) {
                assert(
                    ((std::addressof(explorer.input()) == expected) && ...) &&
                    "all child NeighborhoodExplorers in a union must share the same Instance");
            },
            explorers_);
#endif
    }

    [[nodiscard]]
    const input_type& input() const noexcept
    {
        return std::get<0>(explorers_).input();
    }

    template<std::size_t Index>
        requires(Index < child_count)
    [[nodiscard]]
    std::tuple_element_t<Index, explorer_tuple>& child() noexcept
    {
        return std::get<Index>(explorers_);
    }

    template<std::size_t Index>
        requires(Index < child_count)
    [[nodiscard]]
    const std::tuple_element_t<Index, explorer_tuple>& child() const noexcept
    {
        return std::get<Index>(explorers_);
    }

    template<class Neighborhood>
        requires tuple_contains_type_v<
            Neighborhood,
            std::tuple<logical_neighborhood_type_t<Explorers>...>>
    [[nodiscard]]
    decltype(auto) child() noexcept
    {
        constexpr auto index = tuple_type_index_v<
            Neighborhood,
            std::tuple<logical_neighborhood_type_t<Explorers>...>>;
        return std::get<index>(explorers_);
    }

    template<class Neighborhood>
        requires tuple_contains_type_v<
            Neighborhood,
            std::tuple<logical_neighborhood_type_t<Explorers>...>>
    [[nodiscard]]
    decltype(auto) child() const noexcept
    {
        constexpr auto index = tuple_type_index_v<
            Neighborhood,
            std::tuple<logical_neighborhood_type_t<Explorers>...>>;
        return std::get<index>(explorers_);
    }

    [[nodiscard]]
    bool is_valid(const solution_type& solution, const move_type& move) const
    {
        return std::visit(
            [this, &solution]<class TaggedMove>(const TaggedMove& tagged) {
                constexpr auto index = TaggedMove::index;
                return static_cast<bool>(
                    std::get<index>(explorers_).is_valid(solution, tagged.value));
            },
            move);
    }

    // Moves of different children never forbid each other; between moves of
    // the same child, the child decides.
    [[nodiscard]]
    bool inverse(
        const solution_type& solution,
        const move_type& move,
        const move_type& tabu_move) const
        requires(inverse_neighborhood_for<Explorers, solution_type> && ...)
    {
        if (move.index() != tabu_move.index())
            return false;
        return std::visit(
            [this, &solution, &tabu_move]<class TaggedMove>(const TaggedMove& tagged) {
                constexpr auto index = TaggedMove::index;
                return easylocal::inverse(
                    std::get<index>(explorers_),
                    solution,
                    tagged.value,
                    std::get<index>(tabu_move).value);
            },
            move);
    }

    // The child's attribute, tagged with the child: equal attributes of two
    // children stay distinct.
    [[nodiscard]]
    auto tabu_attribute(const move_type& move) const
        requires(has_tabu_attribute<Explorers> && ...)
    {
        using attribute_type = std::variant<tabu_attribute_t<Explorers>...>;
        return std::visit(
            [this]<class TaggedMove>(const TaggedMove& tagged) {
                constexpr auto index = TaggedMove::index;
                return attribute_type{
                    std::in_place_index<index>,
                    easylocal::tabu_attribute(std::get<index>(explorers_), tagged.value)};
            },
            move);
    }

    [[nodiscard]]
    auto moves(const solution_type& solution) const
        requires (deterministic_neighborhood_for<
                      Explorers, solution_type> && ...)
    {
        return make_moves_view(
            solution,
            std::index_sequence_for<Explorers...>{});
    }

    [[nodiscard]]
    delta_bindings_type delta_bindings() const noexcept
    {
        return make_delta_bindings(
            std::type_identity<common_delta_components>{});
    }

    template<std::uniform_random_bit_generator RNG>
        requires(random_neighborhood_for<Explorers, solution_type, RNG> && ...)
    [[nodiscard]]
    std::optional<move_type> random_move(const solution_type& solution, RNG& rng) const
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

    template<std::uniform_random_bit_generator RNG, class Observer>
        requires(random_neighborhood_for<Explorers, solution_type, RNG> && ...)
    [[nodiscard]]
    std::optional<move_type> random_move_traced(
        const solution_type& solution,
        RNG& rng,
        Observer& observer,
        const trace::neighborhood_route_node* parent = nullptr) const
    {
        std::array<bool, child_count> active{};
        for (std::size_t index = 0; index < active.size(); ++index)
        {
            active[index] = random_biases_[index] > 0.0;
        }

        std::size_t attempt = 0;
        while (true)
        {
            const auto total = active_bias_total(active);
            const auto selected = choose_random_child(active, rng);
            if (!selected)
            {
                return std::nullopt;
            }

            const trace::neighborhood_route_node route{
                .child = *selected,
                .parent = parent,
            };
            auto move = random_move_from_child_traced(
                *selected, solution, rng, observer, &route);

            observer(trace::event::neighborhood_selection{
                .attempt = attempt++,
                .child = *selected,
                .bias = random_biases_[*selected],
                .active_bias_total = total,
                .conditional_probability =
                    total > 0.0 ? random_biases_[*selected] / total : 0.0,
                .produced_move = move.has_value(),
                .neighborhood = &route,
            });

            if (move)
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
    delta_bindings_type make_delta_bindings(
        std::type_identity<std::tuple<Components...>>) const noexcept
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
    }

    [[nodiscard]]
    neighborhood_union_spec with_random_biases(
        std::array<double, sizeof...(Specs)> random_biases) &&
    {
        NeighborhoodUnionParameters<sizeof...(Specs)> parameters{
            .random_biases = std::move(random_biases),
        };
        [[maybe_unused]] const auto validation = configure(std::move(parameters));
        assert(
            validation &&
            "neighborhood random biases must be finite and non-negative");
        return std::move(*this);
    }

    [[nodiscard]]
    const NeighborhoodUnionParameters<sizeof...(Specs)>& parameters() const noexcept
    {
        return parameters_;
    }

    [[nodiscard]]
    config::validation_result configure(
        NeighborhoodUnionParameters<sizeof...(Specs)> parameters) noexcept
    {
        const auto validation = parameters.validate();
        if (!validation)
        {
            return validation;
        }

        parameters_ = std::move(parameters);
        return config::validation_result::success();
    }

    // The biases, at the root, and the parameters of the children under their
    // positions ("0", "1"): a runner puts them under "neighborhood".
    [[nodiscard]]
    config::parameter_set configuration()
    {
        config::parameter_set parameters;
        parameters.add(*this);
        add_child_configurations(parameters, specs_);
        return parameters;
    }

    [[nodiscard]]
    config::parameter_set configuration() const
    {
        config::parameter_set parameters;
        parameters.add(*this);
        add_child_configurations(parameters, specs_);
        return parameters;
    }

    template<class Dependency>
    static constexpr bool constructible_from =
        (Specs::template constructible_from<Dependency> && ...);

    template<class Dependency>
        requires constructible_from<Dependency>
    [[nodiscard]]
    service_type construct(Dependency& dependency) const
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
    template<class Children>
    static void add_child_configurations(
        config::parameter_set& parameters,
        Children& children)
    {
        [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
            (config::add_configuration(
                 parameters,
                 std::to_string(Indices),
                 std::get<Indices>(children)),
                ...);
        }(std::index_sequence_for<Specs...>{});
    }

    template<class Dependency, std::size_t... Indices>
    [[nodiscard]]
    service_type construct_impl(Dependency& dependency, std::index_sequence<Indices...>)
        const
    {
        return service_type{
            parameters_.random_biases,
            std::get<Indices>(specs_).construct(dependency)...,
        };
    }

    std::tuple<Specs...> specs_;
    NeighborhoodUnionParameters<sizeof...(Specs)> parameters_{};
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
