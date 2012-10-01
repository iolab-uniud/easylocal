// $Id: Runner.hh 314 2011-10-04 13:36:41Z sara $
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
#include <stdexcept>

#include <helpers/NeighborhoodExplorer.hh>
#include <utils/Interruptible.hh>
#include <climits>
#include <chrono>
#include <condition_variable>
#include <atomic>
#include <utils/Parameter.hh>


template <class Input, class State, typename CFtype = int>
class Runner : public Interruptible<CFtype, State&>, public Parametrized
{
public:
  
  /** Performs a full run of the search method (possibly being interrupted before its natural ending) on the passed state and returns the cost value after the run. */
  CFtype Go(State& s);
  
  /** Performs a given number of steps of the search method.
  @param n the number of steps to make */	
  CFtype Step(State& s, unsigned int n = 1);
  
  /** @todo */
  virtual std::chrono::milliseconds GetTimeElapsed() const
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin);
  }
  
  virtual void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  
  virtual void Print(std::ostream& os = std::cout) const;
  
  // FIXME: remove parameter accessors
  unsigned long GetMaxIterations() const;
  
  void SetMaxIterations(unsigned long max);
  
  unsigned int IterationOfBest() const { return iteration_of_best; }

  unsigned long int Iteration() const { return iteration; }

  const std::string name;
    
  virtual ~Runner() { }
  
  virtual constexpr unsigned int Modality() const = 0;
  
protected:

  Runner(const Input& i, StateManager<Input,State,CFtype>& sm, std::string name, std::string e_desc);
  
  virtual bool LowerBoundReached() const;
  
  /** Actions and checks to be perfomed at the beginning of the run. */
  virtual void InitializeRun();
  
  /** Actions to be performed at the end of the run. */
  virtual void TerminateRun() = 0;
    
  bool MaxIterationExpired() const;
  
  /** Encodes the criterion used to stop the search. */
  virtual bool StopCriterion() = 0;
  
  /** Encodes the criterion used to select the move at each step. */
  virtual void SelectMove() = 0;
  
  /** Verifies whether the move selected could be performed. */
  virtual bool AcceptableMove();
  
  virtual void PrepareMove() {};
  
  /** Actually performs the move. */
  virtual void MakeMove() = 0;
  
  virtual void CompleteMove() {};
  
  /** Implements Interruptible. */
  virtual std::function<CFtype(State&)> MakeFunction() {
    return [this](State& s) -> CFtype {
      return this->Go(s);
    };
  }
  
  // Helpers
  const Input& in;
  StateManager<Input, State,CFtype>& sm; /**< A pointer to the attached state manager. */
  
  // state data
  State current_state, /**< The current state object. */
  best_state; /**< The best state object. */
  CFtype current_state_cost, /**< The cost of the current state. */
        best_state_cost; /**< The cost of the best state. */
  
  unsigned long int iteration_of_best; /**< The iteration when the best
    state has found. */
  
  unsigned long int iteration; /**< The overall number of iterations
    performed. */
  
  // Parameters
  Parameter<unsigned long int> max_iterations; /**< The maximum number of iterations allowed. */  
  //  unsigned long int max_iterations;
  /** Chronometer. */
  std::chrono::high_resolution_clock::time_point begin;
  std::chrono::high_resolution_clock::time_point end;
  
private:
  
  /** Stores the move and updates the related data. */
  void UpdateBestState();
  
  CFtype TerminateRun(State& s);
  void InitializeRun(State& s);
  
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
Runner<Input,State,CFtype>::Runner(const Input& i, StateManager<Input,State,CFtype>& e_sm, std::string e_name, std::string e_desc)
: Parametrized(e_name, e_desc), name(e_name), in(i), sm(e_sm), current_state(in), best_state(in),
  // parameters
  max_iterations("max_iterations", "Maximum total number of iterations allowed ", this->parameters)
{
  // this parameter has a default value
  max_iterations = std::numeric_limits<unsigned long int>::max();
}

/**
   Returns the maximum value of iterations allowed for the runner.

   @return the maximum value of iterations allowed
*/
// FIXME remove parameter accessors
template <class Input, class State, typename CFtype>
unsigned long Runner<Input,State,CFtype>::GetMaxIterations() const
{
  return max_iterations;
}

/**
   Sets a bound on the maximum number of iterations allowed for the runner.
   
   @param max the maximum number of iterations allowed */
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::SetMaxIterations(unsigned long max)
{
  max_iterations = max;
}

template <class Input, class State, typename CFtype>
CFtype Runner<Input,State,CFtype>::TerminateRun(State& s)
{
  s = best_state;
  end = std::chrono::high_resolution_clock::now();
  TerminateRun();
  return best_state_cost;
}

/**
   Performs a full run of a local search method.
 */
template <class Input, class State, typename CFtype>
CFtype Runner<Input,State,CFtype>::Go(State& s)
{
  InitializeRun(s);
  while (!MaxIterationExpired() && !StopCriterion() && !LowerBoundReached() && !this->TimeoutExpired())
  {
    iteration++;
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
      PrepareMove();
      MakeMove();
      CompleteMove();
      UpdateBestState();
    }
  }
  
  return TerminateRun(s);
}

template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::UpdateBestState()
{
  if (LessOrEqualThan(current_state_cost, best_state_cost))
  {
    best_state = current_state; // Change best_state in case of equal cost to improve diversification
    if (LessThan(current_state_cost, best_state_cost))
    {
      best_state_cost = current_state_cost;
      iteration_of_best = iteration;
    }
  }
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
  return iteration > max_iterations;
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
  // parameter consistency check
}

/**
 These initializations are common to all runner (and cannot be redefined).
 */
template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::InitializeRun(State& s)
{
  begin = std::chrono::high_resolution_clock::now();
  iteration = 0;
  iteration_of_best = 0;
  best_state = current_state = s;
  best_state_cost = current_state_cost = sm.CostFunction(s);
  InitializeRun();
}

template <class Input, class State, typename CFtype>
bool Runner<Input,State,CFtype>::LowerBoundReached() const
{
  return sm.LowerBoundReached(current_state_cost);
}

template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  os << this->name << " -- INPUT PARAMETERS" << std::endl;
  Parametrized::ReadParameters();
}

template <class Input, class State, typename CFtype>
void Runner<Input,State,CFtype>::Print(std::ostream& os) const
{
  os  << "  " << this->name << std::endl;
  Parametrized::Print(os);  
}


#endif // _RUNNER_HH_
