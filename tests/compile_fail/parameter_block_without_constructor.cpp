// An algorithm whose parameters_type is a parameter block is built from it:
// one without that constructor is rejected with the constructor to write.
#include "int_cost_fixture.hpp"

#include <easylocal/runners/first_improvement.hpp>

struct Unbuildable
{
    using parameters_type = easylocal::runners::FirstImprovementParameters;

    template<class Context>
    auto run(const Context& context, typename Context::solution_type solution) const
    {
        return int_cost_fixture::Keep{}.run(context, solution);
    }
};

int main()
{
    auto runner = easylocal::make_runner<Unbuildable>();
    static_cast<void>(runner);
}
