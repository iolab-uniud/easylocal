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

#if !defined(BIMODALTABUSEARCHWITHSHIFTINGPENALTY_HH_)
#define BIMODALTABUSEARCHWITHSHIFTINGPENALTY_HH_

#include <runners/BimodalTabuSearch.hh>

template <class Input, class State, class Move1, class Move2, typename CFtype = int>
class BimodalTabuSearchWithShiftingPenalty
  : public BimodalTabuSearch<Input,State,Move1,Move2,CFtype>
{
public:
  void Print(std::ostream& os = std::cout) const;
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  BimodalTabuSearchWithShiftingPenalty(const Input& in, StateManager<Input,State,CFtype>& s,
				       NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1,
				       NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2,
				       TabuListManager<State, Move1,CFtype>& tlm1,
				       TabuListManager<State, Move2,CFtype>& tlm2,
				       std::string name);
 BimodalTabuSearchWithShiftingPenalty(const Input& in, StateManager<Input,State,CFtype>& s,
				       NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1,
				       NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2,
				       TabuListManager<State, Move1,CFtype>& tlm1,
				       TabuListManager<State, Move2,CFtype>& tlm2,
				      std::string name, CLParser& cl);
  void SetShiftRegion(double sr)
  { shift_region = sr; }
  void SetWeightRegion(double w) { shift_region = w; }
  void InitializeRun(bool first_round = true);
  void SelectMove();
  void MakeMove();
  void StoreMove();
  // for the shifting penalty
  void ResetShifts();
  void UpdateShifts();
  // parameters
  double shift_region;
  bool shifts_reset;
  // command line arguments
  ValArgument<double> arg_shift_region; 
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class State, class Move1, class Move2, typename CFtype>
BimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,CFtype>
::BimodalTabuSearchWithShiftingPenalty(const Input& in, StateManager<Input,State,CFtype>& sm,
				       NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1,
				       NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2,
				       TabuListManager<State, Move1,CFtype>& tlm1,
				       TabuListManager<State, Move2,CFtype>& tlm2,
				       std::string name)
  :  BimodalTabuSearch<Input,State,Move1,Move2,CFtype>(in,sm,ne1,ne2,tlm1,tlm2,name), shift_region(0.75), 
  shifts_reset(false), arg_shift_region("shift_region", "sr", false, 0.75)
{
  
  this->bimodal_tabu_search_arguments.SetAlias("dbts_" + name);
  this->bimodal_tabu_search_arguments.AddArgument(arg_shift_region);
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
BimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,CFtype>
::BimodalTabuSearchWithShiftingPenalty(const Input& in, StateManager<Input,State,CFtype>& sm,
				       NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1,
				       NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2,
				       TabuListManager<State, Move1,CFtype>& tlm1,
				       TabuListManager<State, Move2,CFtype>& tlm2,
				       std::string name, CLParser& cl)
  :  BimodalTabuSearch<Input,State,Move1,Move2,CFtype>(in,sm,ne1,ne2,tlm1,tlm2,name), shift_region(0.75), shifts_reset(false), 
  arg_shift_region("shift_region", "sr", false, 0.75)
{  
  this->bimodal_tabu_search_arguments.SetAlias("dbts_" + name);
  this->bimodal_tabu_search_arguments.AddArgument(arg_shift_region); 
  cl.AddArgument(this->bimodal_tabu_search_arguments);
  cl.MatchArgument(this->bimodal_tabu_search_arguments);
  if (this->bimodal_tabu_search_arguments.IsSet())
  {
    this->pm1.SetLength(this->arg_tabu_tenure_1.GetValue(0), this->arg_tabu_tenure_1.GetValue(1));
    this->pm2.SetLength(this->arg_tabu_tenure_2.GetValue(0), this->arg_tabu_tenure_2.GetValue(1));
    this->max_idle_iteration = this->arg_max_idle_iteration.GetValue();
    shift_region =  arg_shift_region.GetValue();
  }
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,CFtype>::Print(std::ostream& os) const
{
  os  << "Bimodal Tabu Search with Shifting Penalty Runner: " << this->name << std::endl;
  os  << "  Max iterations: " << this->max_iteration << std::endl;
  os  << "  Max idle iteration: " << this->max_idle_iteration << std::endl;
  this->pm1.Print(os);
  this->pm2.Print(os);
  os  << "  Shift region: " << shift_region << std::endl;
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,CFtype>::ResetShifts()
{
  if (!shifts_reset)
    {
      for (unsigned i = 0; i < this->ne1.DeltaCostComponents(); i++)
	this->ne1.DeltaCostComponent(i).ResetShift();
      for (unsigned i = 0; i < this->ne2.DeltaCostComponents(); i++)
	this->ne2.DeltaCostComponent(i).ResetShift();
      shifts_reset = true;
    }
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,CFtype>::UpdateShifts()
{
  if (this->current_move_type == MOVE_1)
    for (unsigned i = 0; i < this->ne1.DeltaCostComponents(); i++)
      this->ne1.DeltaCostComponent(i).UpdateShift(this->current_state);
  else
    for (unsigned i = 0; i < this->ne2.DeltaCostComponents(); i++)
      this->ne2.DeltaCostComponent(i).UpdateShift(this->current_state);
  shifts_reset = false;
}


template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,CFtype>::InitializeRun(bool first_round)
{
  BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::InitializeRun();
  ResetShifts();
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,CFtype>::SelectMove()
{
  bool shifted = (this->number_of_iterations - this->iteration_of_best < shift_region * this->max_idle_iteration);
  if (!shifted)
  {
    BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::SelectMove();
    return;
  }
  Move1 shifted_best_mv1, actual_best_mv1;
  std::pair<ShiftedResult<CFtype>, ShiftedResult<CFtype> > moves_cost1 = 
    this->ne1.BestShiftedMove(this->current_state, shifted_best_mv1, actual_best_mv1, this->pm1);
  
  Move2 shifted_best_mv2, actual_best_mv2;
  std::pair<ShiftedResult<CFtype>, ShiftedResult<CFtype> > moves_cost2 = 
    this->ne2.BestShiftedMove(this->current_state, shifted_best_mv2, actual_best_mv2, this->pm2);
  
  // this is a sort of "Aspiration" for the case one (or both) of the actual_best_move improve over the current best
  if (LessThan(this->current_state_cost + moves_cost1.second.actual_value, this->best_state_cost))
  {
    this->current_move1 = actual_best_mv1;
    this->current_move_cost1 = moves_cost1.second.actual_value;
    if (LessThan(moves_cost1.second.actual_value, moves_cost2.second.actual_value))
      {
	this->current_move_type = MOVE_1;
      }
    else 
      {
	this->current_move2 = actual_best_mv2;
	this->current_move_cost2 = moves_cost2.second.actual_value;
	if (LessThan(moves_cost2.second.actual_value, moves_cost1.second.actual_value))
	  this->current_move_type = MOVE_2;
	else if (EqualTo(moves_cost1.second.actual_value, moves_cost2.second.actual_value))
	  this->current_move_type = Random::Int(0,1) == 0 ? MOVE_1 : MOVE_2;
      }
    return;
  }
  if (LessThan(this->current_state_cost + moves_cost2.second.actual_value, this->best_state_cost))
  {
    this->current_move2 = actual_best_mv2;
    this->current_move_cost2 = moves_cost2.second.actual_value;
    if (LessThan(moves_cost2.second.actual_value, moves_cost1.second.actual_value))
      this->current_move_type = MOVE_2;
    return;
  }
  
  this->current_move1 = shifted_best_mv1;
  this->current_move_cost1 = moves_cost1.first.actual_value;
  this->current_move2 = shifted_best_mv2;
  this->current_move_cost2 = moves_cost2.first.actual_value;
    
  if (LessThan(this->current_move_cost1, this->current_move_cost2))
    this->current_move_type = MOVE_1;
  else if (LessThan(this->current_move_cost2, this->current_move_cost1))
    this->current_move_type = MOVE_2;
  else
    this->current_move_type = Random::Int(0,1) == 0 ? MOVE_1 : MOVE_2;
}


template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,CFtype>::MakeMove()
{
  BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::MakeMove();
  if (this->number_of_iterations - this->iteration_of_best <= shift_region * this->max_idle_iteration)
    UpdateShifts();
  else
    ResetShifts();
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,CFtype>::StoreMove()
{
  if (LessThan(this->current_state_cost, this->best_state_cost))
    ResetShifts();
  BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::StoreMove();
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  os << "BIMODAL TABU SEARCH WITH SHIFTING PENALTY -- INPUT PARAMETERS" << std::endl;
  BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::ReadParameters(is,os);
  os << "  Shift region (fraction of idle iterations): ";
  is >> shift_region;
  for (unsigned i = 0; i < this->ne1.DeltaCostComponents(); i++)
    this->ne1.DeltaCostComponent(i).ReadParameters(is, os);
  for (unsigned i = 0; i < this->ne2.DeltaCostComponents(); i++)
    this->ne2.DeltaCostComponent(i).ReadParameters(is, os);
}
#endif /*BIMODALTABUSEARCHWITHSHIFTINGPENALTY_HH_*/
