// A range's bounds are numbers of one type, or a number and
// easylocal::unlimited: range(0.0, 1) does not compile, where it once picked
// the unlimited overload and failed at run time.
#include <easylocal/config/domain.hpp>

int main()
{
    [[maybe_unused]] constexpr auto domain = easylocal::config::range(0.0, 1);
}
