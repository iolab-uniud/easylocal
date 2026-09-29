#include <easylocal/logging.hpp>

int main()
{
    easylocal::logging::emit(
        easylocal::logging::level::debug,
        "test",
        "header self-containment");
    return 0;
}
