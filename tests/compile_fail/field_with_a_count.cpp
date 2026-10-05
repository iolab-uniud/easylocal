// The domain of a field is a range, one_of or easylocal::unlimited: a count
// does not compile, where it once compiled and threw.
#include <easylocal/config/parameters.hpp>

struct Parameters
{
    int size{1};
};

int main()
{
    [[maybe_unused]] constexpr auto descriptor =
        easylocal::config::field<"size", &Parameters::size>("Size", 5);
}
