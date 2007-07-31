#ifndef SOLVER_HH_
#define SOLVER_HH_

#include "AbstractSolver.hh"
#include "../basics/EasyLocalException.hh"
#include "../helpers/StateManager.hh"
#include "../helpers/OutputManager.hh"
#include "utils/Chronometer.hh"

/** A Solver represents the external layer of EasyLocal++; it
    implements the Abstract Solver interface and furthermore is
    parametrized with the Input and Output of the problem.  @ingroup
    Solvers 
*/
template <class Input, class Output>
class Solver
            : public AbstractSolver
{
public:
    /** Returns the output by translating the best state found by the
        solver to an output object. */
    virtual const Output& GetOutput() = 0;
  virtual void SetTimeout(unsigned int t);
protected:
    Solver(const Input& in);

    const Input& in; /**< A reference to the input manager. */
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
    Constructs a solver by providing it an input manager.

    @param im a reference to the input manager
*/
template <class Input, class Output>
Solver<Input, Output>::Solver(const Input& i)
        : in(i)
{}

template <class Input, class Output>
void Solver<Input, Output>::SetTimeout(unsigned int t)
{
	StoppableObject::SetTimeout(t);
}


#endif /*SOLVER_HH_*/
