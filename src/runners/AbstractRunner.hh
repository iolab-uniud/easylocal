#ifndef ABSTRACTRUNNER_HH_
#define ABSTRACTRUNNER_HH_

#include "../basics/EasyLocalObject.hh"
#include "../basics/StoppableObject.hh"
#include "../basics/EasyLocalException.hh"

/** This is the interface for an abstract runner.
    Each runner has many data objects for representing the state of
    the search (current state, best state, current move, number of
    iterations, ...), and it maintain links to all the helpers,
    which are invoked for performing specific tasks on its own
    data. Example of actual runners are tabu search and 
    simulated annealing.  
    @ingroup Runners
*/
template <class Input, class State, typename CFtype = int>
class AbstractRunner
	: public EasyLocalObject, public StoppableObject
{
public:
    /** Performs a full run of the search method. */
    virtual void Go() throw(EasyLocalException) = 0;
    /** Performs a given number of steps of the search method.
    @param n the number of steps to make */	
    virtual void Step(unsigned int n) throw(EasyLocalException) = 0;
    /** Sets the internal state of the runner to be equal to the
    one passed as parameter.
    @param st the state to become the new runner's state */
    virtual void SetState(const State& st) = 0;
    /** Gets the internal state of the runner.
    @return the internal state of the runner */
    virtual const State& GetState() const = 0;
    /** Gets the cost of the runner's internal state
    @returns the cost value of the runner's internal state. */
    virtual CFtype GetStateCost() const = 0;
    /** Computes the cost of the current state. */
    virtual void ComputeCost() = 0;
    /** Gets the number of iterations performed by the runner.
    @return the number of iterations performed */
    virtual unsigned long GetIterationsPerformed() const = 0;
    /** Checks wether the object state is consistent with all the related
    objects. */
    virtual void Check() const throw(EasyLocalException) = 0;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

#endif /*ABSTRACTRUNNER_HH_*/
