#include "application.hpp"
#include "instance_io.hpp"

#include <easylocal/adapters/tui/tester.hpp>
#include <easylocal/app/tester.hpp>

#ifndef EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE
#error "EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE must name the example instance"
#endif

int main()
{
    easylocal::Tester tester{assignment::make_application("assignment-tester")};
    tester.set_input(assignment::load_instance(EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE));

    easylocal::tui::run(
        tester,
        {
            .title = "EasyLocal Assignment Tester",
            .seed = 0,
            .input_path = EASYLOCAL_ASSIGNMENT_MWE_INSTANCE_FILE,
        });
}
