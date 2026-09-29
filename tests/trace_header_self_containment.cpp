#include <easylocal/trace.hpp>

int main()
{
    easylocal::trace::null_tracer tracer;
    static_assert(!decltype(tracer)::observes<easylocal::trace::event::run_started<int>>);
    return 0;
}
