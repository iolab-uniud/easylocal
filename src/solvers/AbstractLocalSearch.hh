// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2008 Andrea Schaerf, Luca Di Gaspero. 
//
// EasyLocalpp is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// EasyLocalpp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with EasyLocalpp. If not, see <http://www.gnu.org/licenses/>.

#if !defined(_ABSTRACT_LOCAL_SEARCH_HH_)
#define _ABSTRACT_LOCAL_SEARCH_HH_

#include <solvers/Solver.hh>
#include <helpers/StateManager.hh>
#include <helpers/OutputManager.hh>
#include <runners/Runner.hh>
#include <iostream> 
#include <fstream> 
#include <string>
#include <utils/CLParser.hh>

/** A Local Search Solver has an internal state, and defines the ways for
    dealing with a local search algorithm.
    @ingroup Solvers
*/
template <class Input, class Output, class State, typename CFtype = int>
class AbstractLocalSearch
  : public Solver<Input, Output>
{
public:
  void SetInitTrials(unsigned int t);
  CFtype GetCurrentCost() const;
  CFtype GetBestCost() const;
  const Output& GetOutput();
  const State& GetCurrentState() const;
  const State& GetBestState() const;
  virtual void SetCurrentState(const State& st, CFtype cost);
  virtual void SetCurrentState(const State& st);
  virtual void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout) = 0;
protected:
  AbstractLocalSearch(const Input& in,
		      StateManager<Input,State,CFtype>& e_sm,
		      OutputManager<Input,Output,State,CFtype>& e_om,
		      std::string name);
  /** Performs some checking before making a run of the solver,
      if something goes wrong it raises an exception. */
  virtual void FindInitialState(bool random_state = true);
  StateManager<Input,State,CFtype>& sm; /**< A pointer to the attached
					   state manager. */
  OutputManager<Input,Output,State,CFtype>& om; /**< A pointer to the attached
						   output manager. */
  CFtype current_state_cost, best_state_cost;  /**< The cost of the internal states. */
  State current_state, best_state;        /**< The internal states of the solver. */
  unsigned int number_of_init_trials; /**< Number of different initial
					 states tested for a run. */
  Output out;
protected:
  bool LetGo(Runner<Input,State,CFtype>& runner, bool first_round = true);
#if defined(HAVE_PTHREAD)
/**< This variable will be shared among runners (and possibly other lower-level components) and controls their termination. */
RWLockVariable<bool> termination_request, termination_confirmation;
/**< This variable avoids active waiting of runners termination. */
ConditionVariable runner_termination;  
#endif
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   @brief Constructs an abstract local search solver.
  
   @param in an input object
   @param e_sm a compatible state manager
   @param e_om a compatible output manager
   @param name a descriptive name for the solver
*/
template <class Input, class Output, class State, typename CFtype>
AbstractLocalSearch<Input,Output,State,CFtype>::AbstractLocalSearch(const Input& in,
								    StateManager<Input,State,CFtype>& e_sm,
								    OutputManager<Input,Output,State,CFtype>& e_om,  
								    std::string name)
  : Solver<Input, Output>(in, name), sm(e_sm),  om(e_om), current_state(in), best_state(in),
    number_of_init_trials(1), out(in)
{}

template <class Input, class Output, class State, typename CFtype>
const State& AbstractLocalSearch<Input,Output,State,CFtype>::GetCurrentState() const
{ return current_state; }

template <class Input, class Output, class State, typename CFtype>
CFtype AbstractLocalSearch<Input,Output,State,CFtype>::GetCurrentCost() const
{ return current_state_cost; }

template <class Input, class Output, class State, typename CFtype>
const State& AbstractLocalSearch<Input,Output,State,CFtype>::GetBestState() const
{ return best_state; }

template <class Input, class Output, class State, typename CFtype>
CFtype AbstractLocalSearch<Input,Output,State,CFtype>::GetBestCost() const
{ return best_state_cost; }

template <class Input, class Output, class State, typename CFtype>
void AbstractLocalSearch<Input,Output,State,CFtype>::SetCurrentState(const State& st)
{
  current_state = st;
  current_state_cost = sm.CostFunction(current_state); 
}

template <class Input, class Output, class State, typename CFtype>
void AbstractLocalSearch<Input,Output,State,CFtype>::SetCurrentState(const State& st, CFtype cost)
{
  current_state = st;
  current_state_cost = cost; 
}

/**
   Set the number of states which should be tried in 
   the initialization phase.
*/
template <class Input, class Output, class State, typename CFtype>
void AbstractLocalSearch<Input,Output,State,CFtype>::SetInitTrials(unsigned int t)
{
  number_of_init_trials = t;
}

/**
   The output is delivered by converting the best state
   to an output object by means of the output manager.
*/
template <class Input, class Output, class State, typename CFtype>
const Output& AbstractLocalSearch<Input,Output,State,CFtype>::GetOutput() 
{
  om.OutputState(best_state, out);
  return out;
}

/**
   The initial state is generated by delegating this task to 
   the state manager. The function invokes the SampleState function.
*/
template <class Input, class Output, class State, typename CFtype>
void AbstractLocalSearch<Input,Output,State,CFtype>::FindInitialState(bool random_state)
{
  if (random_state)
    current_state_cost = sm.SampleState(current_state, number_of_init_trials);
  else
    {
      sm.GreedyState(current_state);
      current_state_cost = sm.CostFunction(current_state);
    }
}

template <class Input, class Output, class State, typename CFtype>
bool AbstractLocalSearch<Input,Output,State,CFtype>::LetGo(Runner<Input,State,CFtype>& runner, bool first_round)
{
#if defined(HAVE_PTHREAD)
  if (this->timeout_set)
  {
    double time_left;
    termination_request = false;
    termination_confirmation = false;
    runner.SetExternalTerminationVariables(runner_termination, termination_request, termination_confirmation);
    pthread_t runner_thread = runner.GoThread();
    try
    {
      time_left = runner_termination.WaitTimeout(this->current_timeout);
      this->current_timeout = time_left;
      termination_confirmation = true;
    }
    catch (TimeoutExpired e)
    {
      this->current_timeout = 0.0;
      termination_confirmation = true;
    }
    this->termination_request = true;
    pthread_join(runner_thread, NULL);
    runner.ResetExternalTerminationVariables();
    if (this->current_timeout == 0.0)
      return true;
    else
      return false;
  }
  else
  {
    termination_request = false;
    termination_confirmation = true;
    runner.SetExternalTerminationVariables(runner_termination, termination_request, termination_confirmation);
    pthread_t runner_thread = runner.GoThread();
    pthread_join(runner_thread, NULL);
    runner.ResetExternalTerminationVariables();
    return false;
  }
#else
  runner.Go(first_round);
  return false;
#endif
}

#endif // _ABSTRACT_LOCAL_SEARCH_HH_
