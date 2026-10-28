// A field of an enum type needs the names of its values: without an
// easylocal::enumeration beside the enum, the schema does not compile.
#include <easylocal/config/parameters.hpp>

enum class Mode
{
    fast,
    thorough,
};

struct Parameters
{
    easylocal::parameter<Mode, "mode"> mode{Mode::fast};
};

int main()
{
    [[maybe_unused]] constexpr auto descriptor =
        easylocal::config::field<&Parameters::mode>("How it works");
}
