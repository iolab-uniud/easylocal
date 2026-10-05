#pragma once

/// \file
/// Tracer protocol: compile-time event selection and emission.

#include <easylocal/utils/detail/meta.hpp>
#include <easylocal/trace/events.hpp>

#include <concepts>
#include <type_traits>
#include <utility>
#include <variant>

namespace easylocal::trace
{

/// The tracer of a run without tracing: it observes no event, so the search
/// builds none.
struct null_tracer
{
    /// Whether the tracer receives Event: never.
    template<class Event>
    static constexpr bool observes = false;

    /// Discards the event.
    template<class Event>
    constexpr void emit(const Event&) noexcept
    {
    }
};

/// A tracer that says, with `observes<Event>`, whether it receives Event, and
/// that has `emit(event)` when it does.
template<class Tracer, class Event>
concept tracer_for = requires {
    { std::remove_cvref_t<Tracer>::template observes<Event> } ->
        std::convertible_to<bool>;
} && (
    !std::remove_cvref_t<Tracer>::template observes<Event> ||
    requires(Tracer& tracer, const Event& event) {
        tracer.emit(event);
    });

/// A tracer that receives Event.
template<class Tracer, class Event>
concept observes = tracer_for<Tracer, Event> &&
    std::remove_cvref_t<Tracer>::template observes<Event>;

/// Sends value to tracer when it observes the event type, and does nothing,
/// at compile time, otherwise.
///
/// A tracer whose `observes<Event>` is true but that has no `emit()` taking
/// the event is a compile error, rather than an event silently dropped.
template<class Event, class Tracer>
constexpr void emit(Tracer& tracer, const Event& value)
{
    using tracer_type = std::remove_cvref_t<Tracer>;
    static_assert(
        requires {
            { tracer_type::template observes<Event> } -> std::convertible_to<bool>;
        },
        "a tracer says which events it receives with a member "
        "template<class Event> static constexpr bool observes");
    if constexpr (tracer_type::template observes<Event>)
    {
        static_assert(
            requires { tracer.emit(value); },
            "the tracer's observes<Event> is true for this event, but no emit() of "
            "the tracer takes it: check the parameter type of its emit(const Event&), "
            "the cost type included");
        tracer.emit(value);
    }
}

namespace detail
{

template<class Move>
void build_move_route(
    const Move& move,
    const neighborhood_route_node* parent,
    auto&& callback)
{
    if constexpr (easylocal::detail::is_variant_v<Move>)
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

/// Calls callback with the route of move through nested neighborhood unions,
/// null for a move outside a union.
///
/// The route lives on the stack only during the call.
template<class Move, class Callback>
void with_move_route(const Move& move, Callback&& callback)
{
    detail::build_move_route(move, nullptr, std::forward<Callback>(callback));
}

} // namespace easylocal::trace
