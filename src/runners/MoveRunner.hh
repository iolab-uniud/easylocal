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

#if !defined(MOVERUNNER_HH_)
#define MOVERUNNER_HH_

#include <runners/Runner.hh>
#include <helpers/StateManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <observers/RunnerObserver.hh>

/** A Move Runner is an instance of the Runner interface which it compels to
    with a particular definition of @Move (given as template instantiation).
    It is at the root of the inheritance hierarchy of actual runners.
    @ingroup Runners
*/

template <class Input, class State, class Move, typename CFtype = int>
class MoveRunner
: public Runner<Input,State,CFtype>
{
  friend class RunnerObserver<Input,State,Move,CFtype>;
public:
  // Runner interface
  virtual void Check() const;
  void ResetTimeout();
  void AttachObserver(RunnerObserver<Input,State,Move,CFtype>& ob) { observer = &ob; }
  void InitializeRun();
  void TerminateRun();
  Move CurrentMove() const { return current_move; }
  CFtype CurrentMoveCost() const { return current_move_cost; }

protected:
  MoveRunner(const Input& in, StateManager<Input,State,CFtype>& e_sm,
             NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
             std::string name);
  /* state manipulations */
  virtual void GoCheck() const = 0;
  /** Actions to be perfomed at the beginning of the run. */
  virtual void ComputeMoveCost();
  
  /** Encodes the criterion used to select the move at each step. */
  virtual void MakeMove();
  void UpdateStateCost();
  
  NeighborhoodExplorer<Input,State,Move,CFtype>& ne; /**< A pointer to the
    attached neighborhood 
    explorer. */
  
  // state data
  Move current_move;      /**< The currently selected move. */
  CFtype current_move_cost; /**< The cost of the selected move. */

  RunnerObserver<Input,State,Move,CFtype>* observer;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class State, class Move, typename CFtype>
MoveRunner<Input,State,Move,CFtype>::MoveRunner(const Input& in, 
                                                StateManager<Input,State,CFtype>& e_sm,
                                                NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                std::string name)
  : Runner<Input,State,CFtype>(in, e_sm, name), ne(e_ne)
{
  observer = NULL;
}

template <class Input, class State, class Move, typename CFtype>
void MoveRunner<Input,State,Move,CFtype>::InitializeRun() 
{
	Runner<Input,State,CFtype>::InitializeRun();
	if (observer != NULL)
		observer->NotifyStartRunner(*this);
#if defined(HAVE_PTHREAD)
  ne.SetExternalTerminationRequest(this->external_termination_request);
#endif
}


template <class Input, class State, class Move, typename CFtype>
void MoveRunner<Input,State,Move,CFtype>::TerminateRun() 
{
  Runner<Input,State,CFtype>::TerminateRun();
#if defined(HAVE_PTHREAD)
  ne.ResetExternalTerminationRequest();
#endif
}


/**
   Checks wether the object state is consistent with all the related
   objects.
*/
template <class Input, class State, class Move, typename CFtype>
void MoveRunner<Input,State,Move,CFtype>::Check() const
{}

/**
   Actually performs the move selected by the local search strategy.
 */
template <class Input, class State, class Move, typename CFtype>
void MoveRunner<Input,State,Move,CFtype>::MakeMove()
{ 
	ne.MakeMove(this->current_state, current_move);  
}


/**
   Computes the cost of the selected move; it delegates this task to the
   neighborhood explorer.
*/
template <class Input, class State, class Move, typename CFtype>
void MoveRunner<Input,State,Move,CFtype>::ComputeMoveCost()
{ 
	current_move_cost = ne.DeltaCostFunction(this->current_state, current_move); 
}

/**
   Updates the cost of the internal state of the runner.
*/
template <class Input, class State, class Move, typename CFtype>
void MoveRunner<Input,State,Move,CFtype>::UpdateStateCost()
{ 
	this->current_state_cost += current_move_cost; 
}
#endif /*MOVERUNNER_HH_*/
