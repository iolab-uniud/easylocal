// The name of a field is a segment of its path: "cooling rate" or "a.b" does
// not compile.
#include <easylocal/config/parameters.hpp>

struct Parameters
{
    double rate{0.5};
};

int main()
{
    [[maybe_unused]] constexpr auto descriptor =
        easylocal::config::field<"cooling rate", &Parameters::rate>(
            "Rate",
            easylocal::config::range(0.0, 1.0));
}
