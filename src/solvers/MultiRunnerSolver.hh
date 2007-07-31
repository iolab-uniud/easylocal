#ifndef MULTIRUNNERSOLVER_HH_
#define MULTIRUNNERSOLVER_HH_

#include "../helpers/StateManager.hh"
#include "../helpers/OutputManager.hh"
#include "LocalSearchSolver.hh"
#include "../runners/Runner.hh"
#include <vector>

/** A Multi Runner solver handles a set of runners.
    @ingroup Solvers
*/
template <class Input, class Output, class State, typename CFtype = int>
class MultiRunnerSolver
  : public LocalSearchSolver<Input,Output,State,CFtype>
{
public:
  virtual void AddRunner(Runner<Input,State,CFtype>& r);
  void RaiseTimeout();
  void ResetTimeout();
protected:
  MultiRunnerSolver(const Input& i, StateManager<Input, State,CFtype>& sm,
		    OutputManager<Input, Output, State,CFtype>& om, std::string name = "");
  void Check() const throw(EasyLocalException);
  unsigned int start_runner;      /**< The index of the runner to
				     start with. */
  std::vector<Runner<Input,State,CFtype>* > runners; /**< The vector of
						 the linked runners. */
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   Constructs a multi p_kicker->MakeTotalBestKick(this->internal_state)runner solver by providing it links to
   a state manager, an output manager, an input, and an output object.

   @param sm a pointer to a compatible state manager
   @param om a pointer to a compatible output manager
   @param in a pointer to an input object
   @param out a pointer to an output object
*/
template <class Input, class Output, class State, typename CFtype>
MultiRunnerSolver<Input,Output,State,CFtype>::MultiRunnerSolver(const Input& i,
							 StateManager<Input,State,CFtype>& sm,
							 OutputManager<Input,Output,State,CFtype>& om, std::string name)
  : LocalSearchSolver<Input, Output, State,CFtype>(i, sm, om, name),
    start_runner(0), runners(0)
{}

template <class Input, class Output, class State, typename CFtype>
void MultiRunnerSolver<Input,Output,State,CFtype>::RaiseTimeout()
{
  LocalSearchSolver<Input,Output,State,CFtype>::RaiseTimeout();
  for (unsigned int i = 0; i < runners.size(); i++)
    runners[i]->RaiseTimeout();
}

/**
   Adds the given runner to the list of the managed runners.

   @param r a pointer to a compatible runner to add
*/
template <class Input, class Output, class State, typename CFtype>
void MultiRunnerSolver<Input,Output,State,CFtype>::AddRunner(Runner<Input,State,CFtype>& r)
{
  runners.push_back(&r);
}


template <class Input, class Output, class State, typename CFtype>
void MultiRunnerSolver<Input,Output,State,CFtype>::Check() const
  throw(EasyLocalException)
{
  LocalSearchSolver<Input,Output,State,CFtype>::Check();
  if (runners.size() == 0)
    throw EasyLocalException("Check(): runners not set in object " + this->GetName());
  for (unsigned int i = 0; i < this->runners.size(); i++)
    this->runners[i]->Check();
}

#endif /*MULTIRUNNERSOLVER_HH_*/
