#include <easylocal/app.hpp>

int main()
{
    auto application = easylocal::app("self-containment");
    return application.name() == "self-containment" ? 0 : 1;
}
