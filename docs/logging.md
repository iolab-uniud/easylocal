# Logging

!!! note "Experimental"
    The logging API is [Experimental](stability.md): the library itself emits
    no records yet, and the API may change when it does.

EasyLocal Core provides a deliberately small, dependency-free logging boundary in
`<easylocal/utils/logging.hpp>`: records with a level and an origin, sent to one
process-wide sink that an application can route into its own logging system,
without making Core depend on a logging library.

A log `record` contains:

- `level` (`trace`, `debug`, `info`, `warning`, `error`);
- `origin` (`framework` or `application`);
- a category string;
- the message;
- `std::source_location` captured at the call site.

The active sink is a plain `noexcept` function pointer. There is no
`std::function`, virtual dispatch, dynamic allocation, or formatting machinery
in the dispatch path. The default sink writes warnings and errors to `stderr`,
one write per record; installing `nullptr` disables dispatch completely.

```cpp
#include <easylocal/utils/logging.hpp>

void my_sink(const easylocal::logging::record& entry) noexcept
{
    // Bridge to the application's logger here. The string views in entry are
    // valid only for this call.
}

int main()
{
    const auto previous = easylocal::logging::set_sink(&my_sink);

    easylocal::logging::emit(
        easylocal::logging::level::info,
        "application.model",
        "instance loaded");

    easylocal::logging::set_sink(previous);
}
```

The sink is process-wide and is stored atomically so it can be replaced safely.
A sink is invoked synchronously and must not throw; an adapter for a logging
library that may throw should catch exceptions inside the bridge.

The library has no log sites today: the `framework` origin is reserved for
them. When it has some, they will stay on cold paths (configuration,
diagnostics), never in the search loops, so an application that never calls
`emit` pays nothing in a search.
