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
                             std::string name);	
  
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  void Print(std::ostream& os = std::cout) const;
  void SetSteps(unsigned int s) { steps = s; previous_steps.resize(s); }
  
protected:
  void InitializeRun();
  bool AcceptableMove();
  void CompleteMove();
  
  // parameters
  Parameter<unsigned int> steps;
  
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
                                                                                std::string name)
: HillClimbing<Input,State,Move,CFtype>(in, e_sm, e_ne, name),
steps("steps", "ns", this->parameters)

{}

template <class Input, class State, class Move, typename CFtype>
void LateAcceptanceHillClimbing<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
  HillClimbing<Input,State,Move>::Print(os);
  os << "  Steps size: " << steps;
}

/**
 Initializes the run by invoking the companion superclass method, and
 setting the temperature to the start value.
 */
template <class Input, class State, class Move, typename CFtype>
void LateAcceptanceHillClimbing<Input,State,Move,CFtype>::InitializeRun()
{    
  HillClimbing<Input,State,Move,CFtype>::InitializeRun();
  
  // the queue must be filled with the initial state cost at the beginning
  fill(previous_steps.begin(), previous_steps.end(), this->current_state_cost);
}


/**
 A move is randomly picked.
 */
template <class Input, class State, class Move, typename CFtype>
void LateAcceptanceHillClimbing<Input,State,Move,CFtype>::CompleteMove()
{
  previous_steps[this->iteration % steps] = this->best_state_cost;
}

template <class Input, class State, class Move, typename CFtype>
void LateAcceptanceHillClimbing<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)

{
  unsigned step;
  os << "LATE ACCEPTANCE HILL CLIMBING -- INPUT PARAMETERS" << std::endl;
  os << "  Number of idle iterations: ";
  is >> this->max_idle_iterations;
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
  || LessOrEqualThan(this->current_move_cost + this->current_state_cost, previous_steps[this->iteration % steps]);
}

#endif

