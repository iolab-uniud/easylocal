#ifndef ITERATEDLOCALSEARCH_HH_
#define ITERATEDLOCALSEARCH_HH_

#include "SimpleLocalSearch.hh"
#include "../helpers/StateManager.hh"
#include "../helpers/OutputManager.hh"
#include "../runners/Runner.hh"
#include "../kickers/Kicker.hh"

/** An Iterated Local Search solver handles both a runner encapsulating a local
    search algorithm and a kicker used for perturbing current solution.
    @ingroup Solvers */
template <class Input, class Output, class State, typename CFtype = int>
class IteratedLocalSearch
            : public SimpleLocalSearch<Input,Output,State,CFtype>
{
public:
    IteratedLocalSearch(const Input& i,
                        StateManager<Input,State,CFtype>& sm,
                        OutputManager<Input,Output,State,CFtype>& om,
                        std::string name = "Anonymous Iterated Local Search runner");

    void Print(std::ostream& os = std::cout) const;
    void SetKicker(Kicker<Input,State,CFtype>* k);
    void RaiseTimeout();
    void ResetTimeout();
protected:
    void Run();
    void RunCheck() const throw(EasyLocalException);
    Kicker<Input,State,CFtype>* p_kicker; /** A pointer to the managed kicker. */
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
     Constructs an iterated local search solver by providing it links to
     a state manager, an output manager, a runner, a kicker, an input,
     and an output object.

     @param sm a pointer to a compatible state manager
     @param om a pointer to a compatible output manager
  */
template <class Input, class Output, class State, typename CFtype>
IteratedLocalSearch<Input,Output,State,CFtype>::IteratedLocalSearch(const Input& in,
        StateManager<Input,State,CFtype>& sm,
        OutputManager<Input,Output,State,CFtype>& om,
        std::string name)
        : SimpleLocalSearch<Input,Output,State,CFtype>(in, sm, om, name)
{
    p_kicker = NULL;
}

template <class Input, class Output, class State, typename CFtype>
void IteratedLocalSearch<Input,Output,State,CFtype>::RaiseTimeout()
{
    SimpleLocalSearch<Input,Output,State,CFtype>::RaiseTimeout();
    p_kicker->RaiseTimeout();
}

template <class Input, class Output, class State, typename CFtype>
void IteratedLocalSearch<Input,Output,State,CFtype>::Print(std::ostream& os) const
{
    os  << "Iterated Local Search Solver: " << this->GetName() << std::endl;
    if (this->p_runner)
        this->p_runner->Print(os);
    else
        os  << "<no runner attached>" << std::endl;
    if (p_kicker)
        p_kicker->Print(os);
    else
        os  << "<no kicker attached>" << std::endl;
}

template <class Input, class Output, class State, typename CFtype>
void IteratedLocalSearch<Input,Output,State,CFtype>::SetKicker(Kicker<Input,State,CFtype>* k)
{ p_kicker = k; }

/**
   Lets the runner Go, and then collects the best state found.
*/
template <class Input, class Output, class State, typename CFtype>
void IteratedLocalSearch<Input,Output,State,CFtype>::Run()
{
	// TODO: try to minimize the state copying operations
	State current_state(this->in);
	CFtype current_state_cost;
	
    this->p_runner->SetState(this->internal_state);
    // First run
    this->p_runner->Go();
    this->internal_state = this->p_runner->GetState();
    this->internal_state_cost = this->p_runner->GetStateCost();
    // All the time expired in the first run
    if (this->Timeout())
        return;
	// There's some more time
    do
    {
    	current_state = this->internal_state;
    	current_state_cost = this->internal_state_cost;
        // perturb the current solution
        CFtype kick_cost = p_kicker->SelectKick(current_state);
	p_kicker->MakeKick(current_state);
        current_state_cost += kick_cost;
        // and make another run
        this->p_runner->SetState(current_state);
        this->p_runner->Go();
        current_state = this->p_runner->GetState();
        current_state_cost = this->p_runner->GetStateCost();
        if (current_state_cost < this->internal_state_cost)
        {
        	this->internal_state = current_state;
        	this->internal_state_cost = current_state_cost;
        }
    }
    while (current_state_cost < this->internal_state_cost && !this->Timeout());
}

template <class Input, class Output, class State, typename CFtype>
void IteratedLocalSearch<Input,Output,State,CFtype>::RunCheck() const
throw(EasyLocalException)
{
    LocalSearchSolver<Input,Output,State,CFtype>::RunCheck();
    if (p_kicker == NULL)
        throw EasyLocalException("RunCheck(): kicker not set in object " + this->GetName());
}

#endif /*ITERATEDLOCALSEARCH_HH_*/
