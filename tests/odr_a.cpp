// One of the two translation units of easylocal.multi-tu: the umbrella header
// and the headers it leaves out.
#include <easylocal/app/cli.hpp>
#include <easylocal/app/tuning.hpp>
#include <easylocal/easylocal.hpp>
#include <easylocal/testing.hpp>

int easylocal_odr_a()
{
    return 1;
}
