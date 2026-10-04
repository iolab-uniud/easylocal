// The assignment problem as a command-line program: the app of
// application.hpp, run by cli::run, which reads --instance, --runner and the
// runners' parameters (--runners.fi.max_evaluations, ...) and prints the
// result.
#include "application.hpp"
#include "instance_io.hpp" // IWYU pragma: keep (the read_input hook, found by ADL)

#include <easylocal/app/cli.hpp>

#ifndef EASYLOCAL_ASSIGNMENT_INSTANCE_FILE
#error "EASYLOCAL_ASSIGNMENT_INSTANCE_FILE must name the example instance"
#endif

int main(int argc, char* argv[])
{
    // Without switches: the example instance, from its initial solution, with
    // the first runner of the app ("fi").
    return easylocal::cli::run(
        assignment::make_application("assignment"),
        argc,
        argv,
        {.defaults = {
             .instance = EASYLOCAL_ASSIGNMENT_INSTANCE_FILE,
             .start = "initial",
         }});
}
