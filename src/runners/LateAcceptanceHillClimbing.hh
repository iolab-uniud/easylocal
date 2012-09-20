//
//  LateAcceptanceHillClimbing.hh
//  EasyLocal
//
//  Created by Tommaso Urli on 9/13/12.
//  Copyright (c) 2012 University of Udine. All rights reserved.
//

#if !defined(_LATE_ACCEPTANCE_HILL_CLIMBING_HH_)
#define _LATE_ACCEPTANCE_HILL_CLIMBING_HH_

#include <runners/HillClimbing.hh>
#include <helpers/StateManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <stdexcept>

/** The Late Acceptance Hill Climbing maintains a list of previous
 moves and defers acceptance to k steps further.
 
 @ingroup Runners
 */
template <class Input, class State, class Move, typename CFtype = int>
class LateAcceptanceHillClimbing : public HillClimbing<Input,State,Move,CFtype>
{
public:
  
  LateAcceptanceHillClimbing(const Input& in,
                             StateManager<Input,State,CFtype>& e_sm,
                             NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                             std::string name,
                             CLParser& cl = CLParser::empty);	
  
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  void Print(std::ostream& os = std::cout) const;
  void SetSteps(unsigned int s) { steps = s; previous_steps.resize(s); }
  
protected:
  void GoCheck() const;
  void InitializeRun(bool first_round = true);
  void TerminateRun();    
  bool AcceptableMove();
  void StoreMove();
  
  // parameters
  unsigned int steps;
  
  ValArgument<unsigned int> arg_steps;
  
  std::vector<CFtype> previous_steps;
  
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
 Constructs a simulated annealing runner by linking it to a state manager, 
 a neighborhood explorer, and an input object.
 
 @param s a pointer to a compatible state manager
 @param ne a pointer to a compatible neighborhood explorer
 @param in a poiter to an input object
 */
template <class Input, class State, class Move, typename CFtype>
LateAcceptanceHillClimbing<Input,State,Move,CFtype>::LateAcceptanceHillClimbing(const Input& in,
                                                                                StateManager<Input,State,CFtype>& e_sm,
                                                                                NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                                                std::string name,
                                                                                CLParser& cl)
: HillClimbing<Input,State,Move,CFtype>(in, e_sm, e_ne, name),
arg_steps("steps", "ns", true)

{
  SetSteps(10);
  this->hill_climbing_arguments.AddArgument(arg_steps);
  
  cl.MatchArgument(this->hill_climbing_arguments);
	if (this->hill_climbing_arguments.IsSet())
		steps = arg_steps.GetValue();
  
}

template <class Input, class State, class Move, typename CFtype>
void LateAcceptanceHillClimbing<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
  HillClimbing<Input,State,Move>::Print(os);
  os << "  Steps size: " << steps;
}

template <class Input, class State, class Move, typename CFtype>
void LateAcceptanceHillClimbing<Input,State,Move,CFtype>::GoCheck() const
{
  HillClimbing<Input,State,Move,CFtype>::GoCheck();
  if (steps == 0)
    throw std::logic_error("steps is zero for object " + this->name);
}

/**
 Initializes the run by invoking the companion superclass method, and
 setting the temperature to the start value.
 */
template <class Input, class State, class Move, typename CFtype>
void LateAcceptanceHillClimbing<Input,State,Move,CFtype>::InitializeRun(bool first_round)
{    
  HillClimbing<Input,State,Move,CFtype>::InitializeRun(first_round);
  
  // the queue must be filled with the initial state cost at the beginning
  fill(previous_steps.begin(), previous_steps.end(), this->current_state_cost);
}

/**
 At the end of the run, ...
 */
template <class Input, class State, class Move, typename CFtype>
void LateAcceptanceHillClimbing<Input,State,Move,CFtype>::TerminateRun()
{
  if (this->observer != NULL)
    this->observer->NotifyEndRunner(*this);
}

/**
 A move is randomly picked.
 @todo move all common code to superclass HillClimbing.
 */
template <class Input, class State, class Move, typename CFtype>
void LateAcceptanceHillClimbing<Input,State,Move,CFtype>::StoreMove()
{
  if (this->observer != NULL)
    this->observer->NotifyStoreMove(*this);
  
  if (LessOrEqualThan(this->current_state_cost, this->best_state_cost)) 
  {
    this->best_state = this->current_state;      
    if (LessThan(this->current_state_cost, this->best_state_cost))
    {
      if (this->observer != NULL)
        this->observer->NotifyNewBest(*this);      
      this->best_state_cost = this->current_state_cost;
      this->iteration_of_best = this->number_of_iterations;
    }
  }
  previous_steps[this->number_of_iterations % steps] = this->best_state_cost;
}

template <class Input, class State, class Move, typename CFtype>
void LateAcceptanceHillClimbing<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)

{
  unsigned step;
  os << "LATE ACCEPTANCE HILL CLIMBING -- INPUT PARAMETERS" << std::endl;
  os << "  Number of idle iterations: ";
  is >> this->max_idle_iteration;
  os << "  Step size: ";
  is >> step;
  SetSteps(step);
}

/** A move is surely accepted if it improves the cost function
 or with exponentially decreasing probability if it is 
 a worsening one.
 */
template <class Input, class State, class Move, typename CFtype>
bool LateAcceptanceHillClimbing<Input,State,Move,CFtype>::AcceptableMove()
{
  return LessOrEqualThan(this->current_move_cost,(CFtype)0)
  || LessOrEqualThan(this->current_move_cost + this->current_state_cost, previous_steps[this->number_of_iterations % steps]);
}

#endif

