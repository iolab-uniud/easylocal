#ifndef BIMODALTABUSEARCHWITHSHIFTINGPENALTY_HH_
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
				       std::string name = "Anonymous Bimodal Tabu Search With Shifting Penalty");
  void SetShiftRegion(double sr)
  { shift_region = sr; }
  void SetWeightRegion(double w) { shift_region = w; }
  void InitializeRun();
  void SelectMove();
  void MakeMove();
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

template <class Input, class State, class Move1, class Move2, typename CFtype>
BimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,CFtype>
::BimodalTabuSearchWithShiftingPenalty(const Input& in, StateManager<Input,State,CFtype>& sm,
				       NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1,
				       NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2,
				       TabuListManager<State, Move1,CFtype>& tlm1,
				       TabuListManager<State, Move2,CFtype>& tlm2,
				       std::string name)
  :  BimodalTabuSearch<Input,State,Move1,Move2,CFtype>(in,sm,ne1,ne2,tlm1,tlm2,name), shift_region(0.9), shifts_reset(false)
{}

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
void BimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,CFtype>::InitializeRun()
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
    this->ne1.BestShiftedMove(this->current_state, shifted_best_mv1, actual_best_mv1, &this->pm1);
  
  Move2 shifted_best_mv2, actual_best_mv2;
  std::pair<ShiftedResult<CFtype>, ShiftedResult<CFtype> > moves_cost2 = 
    this->ne2.BestShiftedMove(this->current_state, shifted_best_mv2, actual_best_mv2, &this->pm2);
  
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
