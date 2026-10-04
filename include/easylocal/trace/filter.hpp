#pragma once

/// \file
/// A tracer that hides some events from another, chosen at compile time.

#include <easylocal/trace/tracer.hpp>

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
/// event templates. A search does not observe them, so it does not compute
/// them: without solution_visited, for example, no solution hash is computed
/// at each move. The tracer is held by reference.
template<class Tracer, template<class...> class... Excluded>
class without_events
{
public:
    explicit without_events(Tracer& tracer) noexcept : tracer_{tracer} {}

    template<class Event>
    static constexpr bool observes =
        trace::observes<Tracer, Event> && !(detail::is_event_of<Event, Excluded> || ...);

    template<class Event>
        requires observes<Event>
    void emit(const Event& value)
    {
        tracer_.emit(value);
    }

    [[nodiscard]]
    Tracer& tracer() const noexcept
    {
        return tracer_;
    }

private:
    Tracer& tracer_;
};

/// without<event::solution_visited>(recorder): recorder without the visited
/// solutions, for a run that does not build trajectory or local optima
/// networks. The excluded events are the templates of the cost-dependent core
/// events (event::move_evaluated, event::solution_visited, ...).
template<template<class...> class... Excluded, class Tracer>
[[nodiscard]]
without_events<Tracer, Excluded...> without(Tracer& tracer) noexcept
{
    return without_events<Tracer, Excluded...>{tracer};
}

} // namespace easylocal::trace
