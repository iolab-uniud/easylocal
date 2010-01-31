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

#if !defined(_STEEPEST_DESCENT_HH_)
#define _STEEPEST_DESCENT_HH_

#include <runners/MoveRunner.hh>
#include <helpers/StateManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <utils/Types.hh>
#include <utils/CLParser.hh>

/** The Steepest Descent runner performs a simple local search.
    At each step of the search, the best move in the neighborhood of current
    solution is selected and performed.
    It is worth noticing that this algorithm leads straightly to the 
    nearest local minimum of a given state.
    @ingroup Runners
*/  
template <class Input, class State, class Move, typename CFtype = int>
class SteepestDescent
  : public MoveRunner<Input,State,Move,CFtype>
{
public:
  SteepestDescent(const Input& in,
		  StateManager<Input,State,CFtype>& e_sm,
		  NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
		  std::string name);	
  SteepestDescent(const Input& in,
		  StateManager<Input,State,CFtype>& e_sm,
		  NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
		  std::string name,
		  CLParser& cl);		
  SteepestDescent(const Input& in,
                  StateManager<Input,State,CFtype>& e_sm,
                  NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                  std::string name, AbstractTester<Input,State,CFtype>& t);	
  SteepestDescent(const Input& in,
                  StateManager<Input,State,CFtype>& e_sm,
                  NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                  std::string name,
                  CLParser& cl, AbstractTester<Input,State,CFtype>& t);
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  void Print(std::ostream& os = std::cout) const;
protected:
  void GoCheck() const;
  void InitializeRun();
  void TerminateRun();
  void StoreMove();
  bool StopCriterion();
  bool AcceptableMove();
  void SelectMove();
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   Constructs a steepest descent runner by linking it to a state manager, 
   a neighborhood explorer, and an input object.

   @param s a pointer to a compatible state manager
   @param ne a pointer to a compatible neighborhood explorer
   @param in a pointer to an input object
*/
template <class Input, class State, class Move, typename CFtype>
SteepestDescent<Input,State,Move,CFtype>::SteepestDescent(const Input& in,
							  StateManager<Input,State,CFtype>& e_sm, NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
							  std::string name)
  : MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name)
{}

template <class Input, class State, class Move, typename CFtype>
SteepestDescent<Input,State,Move,CFtype>::SteepestDescent(const Input& in,
							  StateManager<Input,State,CFtype>& e_sm, NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
							  std::string name,
							  CLParser& cl)
  : MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name)
{}

template <class Input, class State, class Move, typename CFtype>
SteepestDescent<Input,State,Move,CFtype>::SteepestDescent(const Input& in,
                                                          StateManager<Input,State,CFtype>& e_sm, NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                          std::string name, AbstractTester<Input,State,CFtype>& t)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name, t)
{}

template <class Input, class State, class Move, typename CFtype>
SteepestDescent<Input,State,Move,CFtype>::SteepestDescent(const Input& in,
                                                          StateManager<Input,State,CFtype>& e_sm, NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                          std::string name,
                                                          CLParser& cl, AbstractTester<Input,State,CFtype>& t)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name, t)
{}

template <class Input, class State, class Move, typename CFtype>
void SteepestDescent<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
  os  << "Steepest Descent Runner: " << this->name << std::endl;
  os  << "  Max iterations: " << this->max_iteration << std::endl;
}

/**
   Selects always the best move in the neighborhood.
*/
template <class Input, class State, class Move, typename CFtype>
void SteepestDescent<Input,State,Move,CFtype>::SelectMove()
{
  this->current_move_cost = this->ne.BestMove(this->current_state, this->current_move);
}

/**
   Invokes the companion superclass method, and initializes the move cost
   at a negative value for fulfilling the stop criterion the first time
*/     
template <class Input, class State, class Move, typename CFtype>
void SteepestDescent<Input,State,Move,CFtype>::InitializeRun()
{
  MoveRunner<Input,State,Move,CFtype>::InitializeRun();
  this->current_move_cost = -1; // needed for passing the first time
  // the StopCriterion test
}

template <class Input, class State, class Move, typename CFtype>
void SteepestDescent<Input,State,Move,CFtype>::GoCheck() const

{}

/**
   The search is stopped when no (strictly) improving move has been found.
*/
template <class Input, class State, class Move, typename CFtype>
bool SteepestDescent<Input,State,Move,CFtype>::StopCriterion()
{ return GreaterOrEqualThan<CFtype>(this->current_move_cost,0); }

/**
   A move is accepted if it is an improving one.
*/
template <class Input, class State, class Move, typename CFtype>
bool SteepestDescent<Input,State,Move,CFtype>::AcceptableMove()
{ return LessThan(this->current_move_cost, (CFtype)0); }

template <class Input, class State, class Move, typename CFtype>
void SteepestDescent<Input,State,Move,CFtype>::StoreMove()
{
  if (this->observer != NULL)
    this->observer->NotifyStoreMove(*this);
  if (LessThan(this->current_state_cost, this->best_state_cost))
    {
      if (this->observer != NULL)
	this->observer->NotifyNewBest(*this);
      this->iteration_of_best = this->number_of_iterations;
      this->best_state_cost = this->current_state_cost;
    }
}

/**
   At the end of the run, the best state found is set with the last visited
   state (it is always a local minimum).
*/
template <class Input, class State, class Move, typename CFtype>
void SteepestDescent<Input,State,Move,CFtype>::TerminateRun()
{
  MoveRunner<Input,State,Move,CFtype>::TerminateRun();
  this->best_state = this->current_state;
  this->best_state_cost = this->current_state_cost;
}

template <class Input, class State, class Move, typename CFtype>
void SteepestDescent<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)

{}
#endif // _STEEPEST_DESCENT_HH_
