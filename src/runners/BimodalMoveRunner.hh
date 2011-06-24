// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2011 Andrea Schaerf, Luca Di Gaspero. 
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

#if !defined(_BIMODAL_MOVE_RUNNER_HH_)
#define _BIMODAL_MOVE_RUNNER_HH_

#include <helpers/StateManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <observers/BimodalRunnerObserver.hh>
#include <runners/Runner.hh>
#include <vector>

#if !defined(MOVE_ENUM)
typedef enum {
  MOVE_1 = 1,
  MOVE_2
} PatternMove;
#define MOVE_ENUM
#endif

typedef std::vector<PatternMove> PatternType;

/** A BiMove Runner is an instance of the Runner interface which it compels to
    with two particular definitions of @Move (given as template instantiations).
    It is at the root of the inheritance hierarchy of actual runners.
    @ingroup Runners
*/

template <class Input, class State, class Move1, class Move2, typename CFtype>
class BimodalMoveRunner
  : public Runner<Input,State,CFtype>
{
  friend class BimodalRunnerObserver<Input,State,Move1,Move2,CFtype>;
public:
  // Runner interface
  virtual void Check() const;
  void InitializeRun();
  void TerminateRun();
  void AttachObserver(BimodalRunnerObserver<Input,State,Move1,Move2,CFtype>& ob) { observer = &ob; }
  unsigned int Modality() const { return 2; }
protected:
  BimodalMoveRunner(const Input& im, StateManager<Input,State,CFtype>& sm,
                    NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1,
                    NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2,
                    std::string name);
  /* state manipulations */
  virtual void GoCheck() const = 0;
  /** Actions to be perfomed at the beginning of the run. */
  virtual void ComputeMoveCost();

  void MakeMove();
  void UpdateStateCost();
   

  NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1; /**< A pointer to the
						   attached neighborhood 
						   explorer. */
  NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2; /**< A pointer to the
						   attached neighborhood 
						   explorer. */
  Move1 current_move1;      /**< The currently selected move. */
  Move2 current_move2;      /**< The currently selected move. */
  CFtype current_move_cost1; /**< The cost of the selected move. */
  CFtype current_move_cost2; /**< The cost of the selected move. */
  PatternMove current_move_type;

  BimodalRunnerObserver<Input,State,Move1,Move2,CFtype>* observer;
};

/*  ***** BIMOVE RUNNER ******* */

template <class Input, class State, class Move1, class Move2, typename CFtype>
BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::BimodalMoveRunner(const Input& in,
								     StateManager<Input,State,CFtype>& sm,
								     NeighborhoodExplorer<Input,State,Move1,CFtype>& e_ne1,
								     NeighborhoodExplorer<Input,State,Move2,CFtype>& e_ne2, std::string name)
  : Runner<Input,State,CFtype>(in, sm, name), ne1(e_ne1), ne2(e_ne2)
{   
  observer = NULL;
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::InitializeRun() 
{
  Runner<Input,State,CFtype>::InitializeRun();
  if (observer != NULL)
    observer->NotifyStartRunner(*this);
#if defined(HAVE_PTHREAD)
  if (this->external_termination_request) {
    ne1.SetExternalTerminationRequest(*this->external_termination_request);
    ne2.SetExternalTerminationRequest(*this->external_termination_request);
  }
#endif
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::TerminateRun() 
{
  Runner<Input,State,CFtype>::TerminateRun();
  if (observer != NULL)
    observer->NotifyEndRunner(*this);
#if defined(HAVE_PTHREAD)
  if (this->external_termination_request) {
    ne1.ResetExternalTerminationRequest();
    ne2.ResetExternalTerminationRequest();
  }
#endif
}


/**
   Checks wether the object state is consistent with all the related
   objects.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::Check() const
{
  Runner<Input,State,CFtype>::Check();
}

/**
   Actually performs the move selected by the local search strategy.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::MakeMove()
{
  if (this->current_move_type == MOVE_1)
    ne1.MakeMove(this->current_state, current_move1);
  else
    ne2.MakeMove(this->current_state, current_move2);
}


/**
   Computes the cost of the selected move; it delegates this task to the
   neighborhood explorer.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::ComputeMoveCost()
{
  if (this->current_move_type == MOVE_1)
    current_move_cost1 = ne1.DeltaCostFunction(this->current_state, current_move1);
  else
    current_move_cost2 = ne2.DeltaCostFunction(this->current_state, current_move2);
}

/**
   Updates the cost of the internal state of the runner.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::UpdateStateCost()
{
  if (this->current_move_type == MOVE_1)
    this->current_state_cost += current_move_cost1;
  else
    this->current_state_cost += current_move_cost2;
}

#endif // define _BIMODAL_MOVE_RUNNER_HH_
