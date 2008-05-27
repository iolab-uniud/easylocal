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

#if !defined(TABUSEARCHWITHSHIFTINGPENALTY_HH_)
#define TABUSEARCHWITHSHIFTINGPENALTY_HH_

#include <runners/MoveRunner.hh>
#include <runners/TabuSearch.hh>
#include <helpers/StateManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <helpers/TabuListManager.hh>

template <class Input, class State, class Move, typename CFtype = int>
class TabuSearchWithShiftingPenalty
: public TabuSearch<Input,State,Move,CFtype>
{
public:
TabuSearchWithShiftingPenalty(const Input& in,
                              StateManager<Input,State,CFtype>& sm,
                              NeighborhoodExplorer<Input,State,Move,CFtype>& ne,
                              TabuListManager<State,Move,CFtype>& tlm,
                              std::string name = "Anonymous Tabu Search With Shifting Penalty runner");
void Print(std::ostream& os = std::cout) const;
void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
void SetShiftRegion(double sr)
{ shift_region = sr; }
void SetWeightRegion(double w) { shift_region = w; }
protected:
void InitializeRun();
void SelectMove();
void StoreMove();
// for the shifting penalty
void ResetShifts();
void UpdateShifts();
// parameters
double shift_region;
bool shifts_reset;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class State, class Move, typename CFtype>
TabuSearchWithShiftingPenalty<Input,State,Move,CFtype>::TabuSearchWithShiftingPenalty(const Input& i,
                                                                                      StateManager<Input,State,CFtype>& e_sm,
                                                                                      NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                                                      TabuListManager<State,Move,CFtype>& tlm,
                                                                                      std::string name)
:  TabuSearch<Input,State,Move,CFtype>(i, e_sm, e_ne, tlm, name),
shift_region(0.9), shifts_reset(false)
{}

template <class Input, class State, class Move, typename CFtype>
void TabuSearchWithShiftingPenalty<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
  os  << "Tabu Search with Shifting Penalty Runner: " << this->name << std::endl;
  os  << "  Max iterations: " << this->max_iteration << std::endl;
  os  << "  Max idle iteration: " << this->max_idle_iteration << std::endl;
  this->pm.Print(os);
  os  << "  Shift region: " << shift_region << std::endl;
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearchWithShiftingPenalty<Input,State,Move,CFtype>::ResetShifts()
{
  if (!shifts_reset)
  {
    for (unsigned i = 0; i < this->ne.DeltaCostComponents(); i++)
      this->ne.DeltaCostComponent(i).ResetShift();
    shifts_reset = true;
  }
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearchWithShiftingPenalty<Input,State,Move,CFtype>::UpdateShifts()
{
  for (unsigned i = 0; i < this->ne.DeltaCostComponents(); i++)
    this->ne.DeltaCostComponent(i).UpdateShift(this->current_state);
  shifts_reset = false;
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearchWithShiftingPenalty<Input,State,Move,CFtype>::InitializeRun()
{
  TabuSearch<Input,State,Move,CFtype>::InitializeRun();
  ResetShifts();
}


template <class Input, class State, class Move, typename CFtype>
void TabuSearchWithShiftingPenalty<Input,State,Move,CFtype>::SelectMove()
{
  bool shifted = (this->number_of_iterations - this->iteration_of_best < shift_region * this->max_idle_iteration);
  if (!shifted)
  {
    this->current_move_cost = this->ne.BestMove(this->current_state, this->current_move, this->pm);
    return;
  }
  Move shifted_best_mv, actual_best_mv;
  std::pair<ShiftedResult<CFtype>, ShiftedResult<CFtype> > moves_cost = this->ne.BestShiftedMove(this->current_state, shifted_best_mv, actual_best_mv, this->pm);
  
  if (LessThan(this->current_state_cost + moves_cost.second.actual_value, this->best_state_cost))
  {
    // this is a sort of "Aspiration" for the case the actual_best_move improves over the current best
    this->current_move = actual_best_mv;
    this->current_move_cost =  moves_cost.second.actual_value;
  }
  else
  {
    this->current_move = shifted_best_mv;
    // in all cases the cost should not be the shifted one
    this->current_move_cost = moves_cost.first.actual_value;
  }
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearchWithShiftingPenalty<Input,State,Move,CFtype>::StoreMove()
{
  if (LessThan(this->current_state_cost,this->best_state_cost))
  {
    ResetShifts();
  }
  else if (this->number_of_iterations - this->iteration_of_best < shift_region * this->max_idle_iteration)
  {
    UpdateShifts();
  }
  else
  {
    ResetShifts();
  }
  TabuSearch<Input,State,Move,CFtype>::StoreMove();
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearchWithShiftingPenalty<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  os << "TABU SEARCH WITH SHIFTING PENALTY -- INPUT PARAMETERS" << std::endl;
  TabuSearch<Input,State,Move,CFtype>::ReadParameters(is,os);
  os << "  Shift region (% of idle iterations): ";
  is >> shift_region;
  
  for (unsigned i = 0; i < this->ne.DeltaCostComponents(); i++)
    this->ne.DeltaCostComponent(i).ReadParameters(is, os);
}

#endif /*TABUSEARCHWITHSHIFTINGPENALTY_HH_*/
