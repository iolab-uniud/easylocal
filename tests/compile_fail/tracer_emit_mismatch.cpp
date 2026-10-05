#include <easylocal/trace/events.hpp>
#include <easylocal/trace/tracer.hpp>

#include <concepts>

namespace
{

// Declares that it receives the move_evaluated events of an int cost, but its
// emit() takes those of a long cost: the events would be dropped silently.
struct MismatchedTracer
{
    template<class Event>
    static constexpr bool observes =
        std::same_as<Event, easylocal::trace::event::move_evaluated<int>>;

    void emit(const easylocal::trace::event::move_evaluated<long>&) {}
};

} // namespace

int main()
{
    MismatchedTracer tracer;
    easylocal::trace::emit(tracer, easylocal::trace::event::move_evaluated<int>{});
}
