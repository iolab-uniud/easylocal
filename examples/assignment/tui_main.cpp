// The assignment problem in the TextUI, the interactive tester, on the large
// instance.
#include "application.hpp"

#include <easylocal/adapters/tui/tester.hpp>

#ifndef EASYLOCAL_ASSIGNMENT_INSTANCE_FILE
#error "EASYLOCAL_ASSIGNMENT_INSTANCE_FILE must name the example instance"
#endif

int main()
{
    // The interactive tester loads the Input from input_path (read_input hook).
    easylocal::tui::run(
        assignment::make_application("assignment-tester"),
        {
            .title = "EasyLocal Assignment Tester",
            .seed = 0,
            .input_path = EASYLOCAL_ASSIGNMENT_INSTANCE_FILE,
        });
}
