// A field of a parameter block is a config::parameter: a plain member, whose
// default would be implicit, does not compile.
#include <easylocal/config/parameters.hpp>

struct Parameters
{
    double rate;
};

int main()
{
    [[maybe_unused]] constexpr auto descriptor =
        easylocal::config::field<&Parameters::rate>(
            "Rate",
            easylocal::config::range(0.0, 1.0));
}
