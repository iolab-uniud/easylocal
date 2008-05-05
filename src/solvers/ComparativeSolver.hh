#ifndef COMPARATIVESOLVER_HH_
#define COMPARATIVESOLVER_HH_

#include "MultiRunnerSolver.hh"
#include <helpers/InputManager.hh>
#include <helpers/StateManager.hh>
#include <helpers/OutputManager.hh>

/** A Comparative Solver applies different runners to the same instances
    (and the same initial solutions).
    @ingroup Solvers
*/
template <class Input, class Output, class State, typename CFtype = int>
class ComparativeSolver
: public MultiRunnerSolver<Input,Output,State,CFtype>
{
    /** Constructs a comparative solver by providing it links to
     a state manager, an output manager, an input, and an output object.
		 
     @param sm a pointer to a compatible state manager
     @param om a pointer to a compatible output manager
     @param in a pointer to an input object
     @param out a pointer to an output object
		 */
	ComparativeSolver(const Input& in, StateManager<Input,State,CFtype>& e_sm,
										OutputManager<Input,Output,State,CFtype>& e_om, std::string name = "Anonymous Comparative Solver")
	: MultiRunnerSolver<Input,Output,State,CFtype>(in, e_sm, e_om, name) {}
	void Run();
protected:    
	State start_state; /**< The start state is equal for each runner used
	 and is kept in this variable. */
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
  Lets all the managed runners Go, and then it collects the best state
  found.
*/
template <class Input, class Output, class State, typename CFtype = int>
void ComparativeSolver<Input,Output,State,CFtype>::Run()
{
	bool timeout_expired = false;
	unsigned int i;
	unsigned long num_of_iterations = 0;
	start_state = this->internal_state;
	this->runners[0]->SetState(start_state);
	timeout_expired = LetGo(*this->runners[0]);
	this->runners[0]->ComputeCost();
	this->internal_state = this->runners[0]->GetState();
	this->internal_state_cost = this->runners[0]->StateCost();
	this->chrono.Partial();
	num_of_iterations = this->runners[0]->NumberOfIterations();

	for (i = 1; i < this->runners.size() && !this->Timeout(); i++)
	{
		if (this->plotstream != NULL)
			this->plot.SwitchRunner(this->runners[i]);
		this->chrono.Start();
		this->runners[i]->SetState(start_state,num_of_iterations);
		timeout_expired = LetGo(*this->runners[i]);
		this->runners[i]->ComputeCost();
		this->total_iterations += this->runners[i]->NumberOfIterations();
		if (this->runners[i]->StateCost() < this->internal_state_cost)
		{
			this->internal_state = this->runners[i]->GetState();
			this->internal_state_cost = this->runners[i]->StateCost();
		}
		this->chrono.Partial();
		num_of_iterations = this->runners[i]->NumberOfIterations();
		if (this->logstream != NULL)
		{
			*this->logstream << ">----------------" << std::endl
			<< "Runner " << this->p_runner[i]->GetName()
			<< " has finished" << std::endl
			<< "Runner Iterations elapsed "
			<< num_of_iterations << std::endl
			<< "Time elapsed: "
			<< this->chrono.PartialTime() << 's' << std::endl;
			this->p_sm->PrintStateCost(this->current_state, *this->logstream);
		}
	}
}


#endif /*COMPARATIVESOLVER_HH_*/
