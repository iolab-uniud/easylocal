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

#if !defined(RUNNER_HH_)
#define RUNNER_HH_

#include <EasyLocal.conf.hh>
#include <helpers/StateManager.hh>
#if defined(HAVE_PTHREAD)
#include <utils/Synchronize.hh>
#endif
#include <stdexcept>
#include <utils/Chronometer.hh>

#include <utils/CLParser.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <climits>

#include <climits>

template <class Input, class State, typename CFtype = int>
class Runner
{
public:
  /** Performs a full run of the search method. */
  virtual void Go();
  /** Performs a given number of steps of the search method.
  @param n the number of steps to make */	
  virtual void Step(unsigned int n);
  /** Sets the internal state of the runner to be equal to the
    one passed as parameter.
    @param st the state to become the new runner's state */
  virtual void SetState(const State& st);
  /** Sets the internal state of the runner to be equal to the
    one passed as parameter and updates the cost with the value of the @c cost variable.
    @param st the state to become the new runner's state 
    @param cost the state cost
    */  
  virtual void SetState(const State& st, CFtype cost);
  /** Gets the internal state of the runner.
    @return the internal state of the runner */
  virtual const State& GetState() const;
  /** Gets the cost of the runner's internal state
    @returns the cost value of the runner's internal state. */
  virtual CFtype GetStateCost() const;
  /** Gets the best state of the runner.
    @return the internal state of the runner */
  virtual void ComputeCost();
  /** Gets the number of iterations performed by the runner.
    @return the number of iterations performed */
  virtual unsigned long GetIterationsPerformed() const;
  /** Checks wether the object state is consistent with all the related
    objects. */
  virtual void Check() const;
  virtual void Print(std::ostream& os = std::cout) const = 0;
  unsigned long GetMaxIteration() const;
  void SetMaxIteration(unsigned long max);
  virtual void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout) = 0;
  bool LowerBoundReached() const;

  unsigned IterationOfBest() const { return iteration_of_best; }
  unsigned NumberOfIterations() const { return number_of_iterations; }

  const std::string name;
protected:
  Runner(const Input& i, StateManager<Input,State,CFtype>& sm, std::string name);
  virtual ~Runner() {}
  /* state manipulations */
  virtual void GoCheck() const = 0;
  /** Actions to be perfomed at the beginning of the run. */
  virtual void InitializeRun();
  /** Actions to be performed at the end of the run. */
  virtual void TerminateRun();
  virtual void UpdateIterationCounter();
  bool MaxIterationExpired() const;
  bool ExternalTerminationRequest() const;
  /** Encodes the criterion used to stop the search. */
  virtual bool StopCriterion() = 0;
  /** Encodes the criterion used to select the move at each step. */
  virtual void SelectMove() = 0;
  /** Verifies whether the move selected could be performed. */
  virtual bool AcceptableMove();
  /** Actually performs the move. */
  virtual void MakeMove() = 0;
  /** Stores the move and updates the related data. */
  virtual void StoreMove() = 0;
  virtual void UpdateStateCost() = 0;
  // input
  const Input& in; /**< A pointer to the input object. */
  // helpers
  StateManager<Input, State,CFtype>& sm; /**< A pointer to the attached
    state manager. */
  
  // state data
  State current_state, /**< The current state object. */
    best_state; /**< The best state object. */
  CFtype current_state_cost, /**< The cost of the current state. */
    best_state_cost; /**< The cost of the best state. */
  bool current_state_set; /**< A flag that whether the current state is set.
    It is so until a new input is given. */  
  
  unsigned long iteration_of_best; /**< The iteration when the best
    state has found. */
  unsigned long number_of_iterations; /**< The overall number of iterations
    performed. */
  unsigned long start_iteration;
  unsigned long max_iteration; /**< The maximum number of iterations
    allowed. */
#if defined(HAVE_PTHREAD)  
protected:
  ConditionVariable* termination;
  RWLockVariable<bool> *external_termination_request, *external_termination_confirmation;
public:
  virtual void SetExternalTerminationVariables(ConditionVariable& termination, RWLockVariable<bool>& external_termination_request,
                                               RWLockVariable<bool>& external_termination_confirmation);
  virtual void ResetExternalTerminationVariables();
  pthread_t& GoThread();
private:
  static void* _pthreads_Run(void* pthis);
  pthread_t this_thread;
#endif
protected:
  Chronometer chrono;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
  Creates a move runner and links it to a given state manager, neighborhood
  explorer and input objects. In addition, it sets its name and type to
  the given values.

  @param sm a pointer to a compatible state manager
  @param ne a pointer to a compatible neighborhood explorer
  @param in a pointer to the input object
  @param name the name of the runner
*/
template <class Input, class State, typename CFtype>
Runner<Input,State,CFtype>::Runner(const Input& i, StateManager<Input,State,CFtype>& e_sm, std::string e_name)
: name(e_name), in(i), sm(e_sm), current_state(i), best_state(i), current_state_set(false), number_of_iterations(0), max_iteration(ULONG_MAX)
#if defined(HAVE_PTHREAD)
,  termination(NULL), external_termination_request(NULL)
#endif
{}

/**
   Checks whether the object state is consistent with all the related
   objects.
*/
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::Check() const
{}

/**
   Sets the internal state of the runner to the value passed as parameter.

   @param s the state to become the current state of the runner
*/
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::SetState(const State& s)
{
  current_state = s;
  current_state_cost = sm.CostFunction(current_state);
  current_state_set = true;
  best_state = current_state;
  best_state_cost = current_state_cost;
}

/**
   Sets the internal state of the runner to the value passed as parameter.

   @param s the state to become the current state of the runner
*/
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::SetState(const State& s, CFtype cost)
{
  current_state = s;
  current_state_cost = cost;
  current_state_set = true;
  best_state = current_state;
  best_state_cost = current_state_cost;
}

/**
   Retrieves the state of the runner.
   
   @return the current state of the runner
*/
template <class Input, class State, typename CFtype>
const State& Runner<Input,State,CFtype>::GetState() const
{
  return best_state;
}

/**
    Returns the cost of the state
    @return the cost of the state
*/
template <class Input, class State, typename CFtype>
CFtype Runner<Input,State,CFtype>::GetStateCost() const
{
  return best_state_cost;
}

/**
    Computes explicitely the cost of the current state (used 
    at the beginning of a run for consistency purpose).
*/
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::ComputeCost()
{
  current_state_cost = sm.CostFunction(current_state);
}

/**
   Returns the number of iterations executed.

   @return the number of iterations performed by the runner
*/
template <class Input, class State, typename CFtype>
unsigned long Runner<Input,State,CFtype>::GetIterationsPerformed() const
{
  return number_of_iterations;
}

/**
   Returns the maximum value of iterations allowed for the runner.

   @return the maximum value of iterations allowed
*/
template <class Input, class State, typename CFtype>
unsigned long Runner<Input,State,CFtype>::GetMaxIteration() const
{
  return max_iteration;
}

/**
   Sets a bound on the maximum number of iterations allowed for the runner.
   
   @param max the maximum number of iterations allowed */
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::SetMaxIteration(unsigned long max)
{
  max_iteration = max;
}

template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::TerminateRun()
{	
  chrono.Stop();
}

/**
   Performs a full run of a local search method.
 */
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::Go()

{
  GoCheck();
  InitializeRun();
  while (!ExternalTerminationRequest()  && 
         !MaxIterationExpired() && 
         !StopCriterion() && !LowerBoundReached())
    {
      UpdateIterationCounter();
      try 
	{
	  SelectMove();
	}
      catch (EmptyNeighborhood e)
	{
	  break; // If the neighborhood is empty, the search will be stopped
	}
      if (AcceptableMove())
	{
	  MakeMove();
	  UpdateStateCost();
	  StoreMove();
	}
    }
  TerminateRun();
}

template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::GoCheck() const

{
  if (!current_state_set)
    throw std::logic_error("Current State not set in runner object " + this->name);
}


/**
   Performs a given number of steps of the local search strategy.

   @param n the number of steps
*/
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::Step(unsigned int n)

{
  GoCheck();
  for (unsigned int i = 0; i < n; i++)
  {
    UpdateIterationCounter();
    SelectMove();
    if (AcceptableMove())
    {
      MakeMove();
      UpdateStateCost();
      StoreMove();
      if (ExternalTerminationRequest() || LowerBoundReached())
        break;
    }
  }
}

/**
   Updates the counter that tracks the number of iterations elapsed.
*/
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::UpdateIterationCounter()
{
  number_of_iterations++;
}

/**
   Verifies whether the upper bound on the number of iterations
   allowed for the strategy has been reached.

   @return true if the maximum number of iteration has been reached, false
   otherwise
*/
template <class Input, class State, typename CFtype>
bool Runner<Input,State,CFtype>::MaxIterationExpired() const
{
  return number_of_iterations > max_iteration;
}

template <class Input, class State, typename CFtype>
inline bool Runner<Input,State,CFtype>::ExternalTerminationRequest() const
{
#if defined(HAVE_PTHREAD)  
  if (external_termination_request)
    return *external_termination_request;
  else
    return false;
#else
  return false;
#endif
}


/**
    Checks whether the selected move can be performed.
    Its tentative definition simply returns true
*/
template <class Input, class State, typename CFtype>
bool Runner<Input,State,CFtype>::AcceptableMove()
{
  return true;
}

/**
   Initializes all the runner variable for starting a new run.
*/
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::InitializeRun()
{
  number_of_iterations = 0;
  iteration_of_best = 0;
  ComputeCost();
  chrono.Reset();
  chrono.Start();
}

template <class Input, class State, typename CFtype>
bool Runner<Input,State,CFtype>::LowerBoundReached() const
{
  return sm.LowerBoundReached(current_state_cost);
}

#if defined(HAVE_PTHREAD)
template <class Input, class State, typename CFtype>
void* Runner<Input,State,CFtype>::_pthreads_Run(void* ep_this)
{
  Runner<Input,State,CFtype>* p_this = static_cast<Runner<Input,State,CFtype>*>(ep_this);
  if (!p_this)
    return NULL;
    
  p_this->Go();
  if (p_this->termination && !(*p_this->external_termination_request))
  do
    p_this->termination->Signal();
  while (!(*p_this->external_termination_confirmation));
  p_this->ResetExternalTerminationVariables();
  
  return NULL;
}

template <class Input, class State, typename CFtype>
pthread_t& Runner<Input,State,CFtype>::GoThread()
{
  pthread_create(&this_thread, NULL, Runner<Input,State,CFtype>::_pthreads_Run, this);
	
  return this_thread;
}

#if defined(HAVE_PTHREAD)
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::SetExternalTerminationVariables(ConditionVariable& r_termination, 
                                                                 RWLockVariable<bool>& r_external_termination_request,
                                                                 RWLockVariable<bool>& r_external_termination_confirmation)
{
  this->termination = &r_termination;
  this->external_termination_request = &r_external_termination_request;	
  this->external_termination_confirmation = &r_external_termination_confirmation;
}

template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::ResetExternalTerminationVariables()
{
  this->termination = NULL;
  this->external_termination_request = NULL;	
  this->external_termination_confirmation = NULL;
}
#endif


#endif

#endif /*RUNNER_HH_*/
