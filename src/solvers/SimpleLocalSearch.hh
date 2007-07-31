#ifndef SIMPLELOCALSEARCH_HH_
#define SIMPLELOCALSEARCH_HH_

#include "../helpers/StateManager.hh"
#include "../helpers/OutputManager.hh"
#include "LocalSearchSolver.hh"
#include "../runners/Runner.hh"
#ifdef EASYLOCAL_PTHREADS
#include <pthread.h>
#endif


/** The Simple Local Search solver handles a simple local search algorithm
    encapsulated in a runner.
    @ingroup Solvers
*/
template <class Input, class Output, class State, typename CFtype = int>
class SimpleLocalSearch
            : public LocalSearchSolver<Input,Output,State,CFtype>
{
public:
    SimpleLocalSearch(const Input& in,
                      StateManager<Input,State,CFtype>& e_sm,
                      OutputManager<Input,Output,State,CFtype>& e_om,
                      const std::string& name = "Anonymous Simple Local Search solver");

    void Print(std::ostream& os = std::cout) const;
    void SetRunner(Runner<Input,State,CFtype>& r);
    void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout); 
    void RaiseTimeout();
    
protected:
    void Run();
    void Check() const throw(EasyLocalException);
    Runner<Input,State,CFtype>* p_runner; /**< A pointer to the managed runner. */
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   Constructs a simple local search solver by providing it links to
   a state manager, an output manager, a runner, an input,
   and an output object.

   @param sm a pointer to a compatible state manager
   @param om a pointer to a compatible output manager
   @param r a pointer to a compatible runner
   @param in a pointer to an input object
   @param out a pointer to an output object
*/
template <class Input, class Output, class State, typename CFtype>
SimpleLocalSearch<Input,Output,State,CFtype>::SimpleLocalSearch(const Input& in,
        StateManager<Input,State,CFtype>& e_sm,
        OutputManager<Input,Output,State,CFtype>& e_om,
        const std::string& name)
        : LocalSearchSolver<Input,Output,State,CFtype>(in, e_sm, e_om, name)
{
	p_runner = NULL;
}

template <class Input, class Output, class State, typename CFtype>
void SimpleLocalSearch<Input,Output,State,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
	os << "Simple Local Search Solver: " << this->GetName() << " parameters" << std::endl;
	os << "Runner: " << std::endl; 
	if (this->p_runner)
		this->p_runner->ReadParameters(is, os);
	os << "Timeout: ";
	is >> this->timeout;
}



template <class Input, class Output, class State, typename CFtype>
void SimpleLocalSearch<Input,Output,State,CFtype>::Print(std::ostream& os) const
{
    os  << "Simple Local Search Solver: " << this->GetName() << std::endl;
    if (this->p_runner)
        this->p_runner->Print(os);
    else
        os  << "<no runner attached>" << std::endl;
}


/**
    Sets the runner employed for solving the problem to the one passed as
    parameter.

    @param r the new runner to be used
 */
template <class Input, class Output, class State, typename CFtype>
void SimpleLocalSearch<Input,Output,State,CFtype>::SetRunner(Runner<Input,State,CFtype>& r)
{ this->p_runner = &r; }


template <class Input, class Output, class State, typename CFtype>
void SimpleLocalSearch<Input,Output,State,CFtype>::RaiseTimeout()
{
    LocalSearchSolver<Input,Output,State,CFtype>::RaiseTimeout();
    this->p_runner->RaiseTimeout();
}

/**
   Lets the runner Go, and then collects the best state found.
*/
template <class Input, class Output, class State, typename CFtype>
void SimpleLocalSearch<Input,Output,State,CFtype>::Run()
{
#ifndef EASYLOCAL_PTHREADS
    this->p_runner->SetState(this->internal_state);
    this->p_runner->Go();
    this->internal_state = this->p_runner->GetState();
    this->internal_state_cost = this->p_runner->GetStateCost();
#if VERBOSE >= 2
      std::cerr << "Runner, cost: " << this->p_runner->GetStateCost() 
		<< ", distance " << this->sm.StateDistance( this->internal_state, this->p_runner->GetState())
		<< " (" << this->p_runner->GetIterationsPerformed() << " iterations"
		<< ")" << std::endl;
#endif
#else
    pthread_t runner_thd_id;
    this->p_runner->SetState(this->internal_state);
    this->SetTimer();
    runner_thd_id = this->p_runner->GoThread();
    unsigned int waiting_time = this->timeout;
    while (!this->p_runner->WaitTermination(waiting_time) && !this->Timeout())
#ifdef DEBUG_PTHREADS
    	std::cerr << "CHECKING TERMINATION" << std::endl;
#else
		;
#endif
    pthread_join(runner_thd_id, NULL);
#endif
}

template <class Input, class Output, class State, typename CFtype>
void SimpleLocalSearch<Input,Output,State,CFtype>::Check() const
throw(EasyLocalException)
{
    LocalSearchSolver<Input,Output,State,CFtype>::Check();
    if (this->p_runner == NULL)
      throw EasyLocalException("Check(): runner not set in object " + this->GetName());
    this->p_runner->Check();
}

#endif /*SIMPLELOCALSEARCH_HH_*/
