#include <easylocal/run_control.hpp>

#include <stop_token>

int main()
{
    std::stop_source stop;
    auto observer = [](const easylocal::run_progress&) {};
    const easylocal::run_control control{stop.get_token(), observer};
    return control.stop_requested() ? 1 : 0;
}
