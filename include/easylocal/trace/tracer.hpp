#pragma once

#include <easylocal/trace/events.hpp>

#include <concepts>
#include <type_traits>
#include <utility>
#include <variant>

// Tracer protocol: compile-time event selection and emission.
namespace easylocal::trace
{

struct null_tracer
{
    template<class Event>
    static constexpr bool observes = false;

    template<class Event>
    constexpr void emit(const Event&) noexcept
    {
    }
};

template<class Tracer, class Event>
concept tracer_for = requires {
    { std::remove_cvref_t<Tracer>::template observes<Event> } ->
        std::convertible_to<bool>;
} && (
    !std::remove_cvref_t<Tracer>::template observes<Event> ||
    requires(Tracer& tracer, const Event& event) {
        tracer.emit(event);
    });

template<class Tracer, class Event>
concept observes = tracer_for<Tracer, Event> &&
    std::remove_cvref_t<Tracer>::template observes<Event>;

template<class Event, class Tracer>
constexpr void emit(Tracer& tracer, const Event& value)
{
    if constexpr (observes<Tracer, Event>)
    {
        tracer.emit(value);
    }
}

namespace detail
{

template<class T>
struct is_variant : std::false_type
{
};

template<class... Ts>
struct is_variant<std::variant<Ts...>> : std::true_type
{
};

template<class T>
inline constexpr bool is_variant_v = is_variant<std::remove_cvref_t<T>>::value;

template<class Move>
void build_move_route(
    const Move& move,
    const neighborhood_route_node* parent,
    auto&& callback)
{
    if constexpr (is_variant_v<Move>)
    {
        std::visit(
            [&](const auto& tagged) {
                using tagged_type = std::remove_cvref_t<decltype(tagged)>;
                if constexpr (requires { tagged_type::index; tagged.value; })
                {
                    const neighborhood_route_node node{
                        .child = tagged_type::index,
                        .parent = parent,
                    };
                    build_move_route(tagged.value, &node, callback);
                }
                else
                {
                    callback(parent);
                }
            },
            move);
    }
    else
    {
        callback(parent);
    }
}

} // namespace detail

template<class Move, class Callback>
void with_move_route(const Move& move, Callback&& callback)
{
    detail::build_move_route(move, nullptr, std::forward<Callback>(callback));
}

} // namespace easylocal::trace
