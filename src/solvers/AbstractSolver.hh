#ifndef ABSTRACTSOLVER_HH_
#define ABSTRACTSOLVER_HH_

#include "../basics/RegistrableObject.hh"
#include "../basics/StoppableObject.hh"
#include "../basics/EasyLocalException.hh"

/** An Abstract Solver is an abstract interface for the Solver concept.
    It simply defines the signature for them, independently of the
    problem definition classes.
    @ingroup Solvers
*/
class AbstractSolver
            : virtual public RegistrableObject, public StoppableObject
{
public:
    /** Performs a full solving procedure by finding an initial state,
     running the attached runner and delivering the output. 
     Furthermore it returns the CPU time elapsed in the solving procedure.
    */    
    virtual void Solve() throw(EasyLocalException) = 0;
    /** Start again a solving procedure, running the attached runner from
    the current internal state. */
    virtual void ReSolve() throw(EasyLocalException) = 0;
    /** Tries multiple runs on different initial states and records the
    best one.
    @param n the number of trials  
    @param output_prefix the output filename prefix
    */
    virtual void MultiTrialSolve(unsigned int n) throw(EasyLocalException) = 0;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

#endif /*ABSTRACTSOLVER_HH_*/
