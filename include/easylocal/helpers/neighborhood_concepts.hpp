#pragma once

#include <concepts>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>

namespace easylocal
{

namespace detail
{

template<class T>
struct optional_value;

template<class T>
struct optional_value<std::optional<T>>
{
    using type = T;
};

template<class T>
using optional_value_t = typename optional_value<std::remove_cvref_t<T>>::type;

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

} // namespace easylocal
