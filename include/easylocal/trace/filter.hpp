#pragma once

/// \file
/// A tracer that hides some events from another, chosen at compile time.

#include <easylocal/trace/tracer.hpp>

#include <concepts>

namespace easylocal::trace
{

namespace detail
{

template<class Event, template<class...> class Template>
inline constexpr bool is_event_of = false;

template<template<class...> class Template, class... Arguments>
inline constexpr bool is_event_of<Template<Arguments...>, Template> = true;

} // namespace detail

/// Forwards to tracer the events it observes, except those of the Excluded
/// event templates.
///
/// A search does not observe them, so it does not compute them: without
/// solution_visited, for example, no solution hash is computed at each move.
/// The tracer is held by reference.
template<class Tracer, template<class...> class... Excluded>
class without_events
{
public:
    /// The wrapper of tracer.
    explicit without_events(Tracer& tracer) noexcept : tracer_{tracer} {}

    /// Whether the wrapper receives Event: when the tracer observes it and it
    /// is not an instance of an Excluded template.
    template<class Event>
    static constexpr bool observes =
        trace::observes<Tracer, Event> && !(detail::is_event_of<Event, Excluded> || ...);

    /// Forwards value to the tracer.
    template<class Event>
        requires observes<Event>
    void emit(const Event& value)
    {
        tracer_.emit(value);
    }

    /// The wrapped tracer.
    [[nodiscard]]
    Tracer& tracer() const noexcept
    {
        return tracer_;
    }

private:
    Tracer& tracer_;
};

/// Forwards to tracer the events it observes, except those of the Excluded
/// event types, such as event::neighborhood_selection.
///
/// A search does not observe them, so it does not compute them. The tracer
/// is held by reference.
template<class Tracer, class... Excluded>
class without_event_types
{
public:
    explicit without_event_types(Tracer& tracer) noexcept : tracer_{tracer} {}

    /// Whether the wrapper receives Event: when the tracer observes it and it
    /// is not one of the Excluded types.
    template<class Event>
    static constexpr bool observes =
        trace::observes<Tracer, Event> && !(std::same_as<Event, Excluded> || ...);

    /// Forwards value to the tracer.
    template<class Event>
        requires observes<Event>
    void emit(const Event& value)
    {
        tracer_.emit(value);
    }

    /// The wrapped tracer.
    [[nodiscard]]
    Tracer& tracer() const noexcept
    {
        return tracer_;
    }

private:
    Tracer& tracer_;
};

/// `without<event::solution_visited>(recorder)`: recorder without the visited
/// solutions, for a run that does not build trajectory or local optima
/// networks.
///
/// The excluded events are the templates of the cost-dependent core events
/// (event::move_evaluated, event::solution_visited, ...); the events without
/// a cost are excluded by their type, with the other without().
template<template<class...> class... Excluded, class Tracer>
[[nodiscard]]
without_events<Tracer, Excluded...> without(Tracer& tracer) noexcept
{
    return without_events<Tracer, Excluded...>{tracer};
}

/// `without<event::neighborhood_selection>(recorder)`: recorder without the
/// events of the given types, the core events without a cost
/// (event::neighborhood_selection, event::tabu_escape, ...) or application
/// events.
///
/// A wrapper is a tracer, so the two forms combine:
/// `without<event::move_evaluated>(unselected)` for an `unselected` built by
/// this one.
template<class... Excluded, class Tracer>
[[nodiscard]]
without_event_types<Tracer, Excluded...> without(Tracer& tracer) noexcept
{
    return without_event_types<Tracer, Excluded...>{tracer};
}

} // namespace easylocal::trace
