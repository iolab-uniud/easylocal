#ifndef BIMODALTABUSEARCHWITHSHIFTINGPENALTY_HH_
#define BIMODALTABUSEARCHWITHSHIFTINGPENALTY_HH_

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
  void InitializeRun();
  void SelectMove1();
  void SelectMove2();
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
  os  << "Bimodal Tabu Search with Shifting Penalty Runner: " << this->GetName() << std::endl;
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
void BimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,CFtype>::SelectMove1()
{
  bool shifted = (this->number_of_iterations - this->iteration_of_best < shift_region * this->max_idle_iteration);
  Move1 mv;
  ShiftedResult<CFtype> mv_cost;
  bool all_moves_tabu = true;
	
  this->ne1.FirstMove(this->current_state, mv);
  mv_cost = this->ne1.DeltaShiftedCostFunction(this->current_state, mv);
  if (!shifted)
    mv_cost.shifted_value = (double)mv_cost.actual_value;
  Move1 best_move = mv;
  ShiftedResult<CFtype> best_delta = mv_cost;

  do  // look for the best non prohibited move
    {   // (if all moves are prohibited, then get the best)
      // For efficency, ProhibitedMove is invoked only when strictly necessary
      if (LessThan(mv_cost.shifted_value,best_delta.shifted_value))
	{
	  if (!this->pm1.ProhibitedMove(this->current_state, mv, mv_cost.actual_value))
	    {
	      best_move = mv;
	      best_delta = mv_cost;
	      all_moves_tabu = false;
	    }
	  if (all_moves_tabu)
	    {
	      best_move = mv;
	      best_delta = mv_cost;
	    }
	}
      else if (all_moves_tabu && !this->pm1.ProhibitedMove(this->current_state, mv, mv_cost.actual_value))
	{ // even though it is not an improving move,
	  // this move is the actual best since it's the first non-tabu
	  best_move = mv;
	  best_delta = mv_cost;
	  all_moves_tabu = false;
	}
      this->ne1.NextMove(this->current_state, mv);
      mv_cost = this->ne1.DeltaShiftedCostFunction(this->current_state, mv);
      if (!shifted)
	mv_cost.shifted_value = (double)mv_cost.actual_value;
    }
  while (!this->ne1.LastMoveDone(this->current_state, mv));
	
  this->current_move1 = best_move;
  // in all cases the cost should not be the shifted one
  this->current_move_cost1 = this->ne1.DeltaCostFunction(this->current_state, best_move);
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,CFtype>::SelectMove2()
{
  bool shifted = (this->number_of_iterations - this->iteration_of_best < shift_region * this->max_idle_iteration);
  Move2 mv;
  ShiftedResult<CFtype> mv_cost;
  bool all_moves_tabu = true;
	
  this->ne2.FirstMove(this->current_state, mv);
  mv_cost = this->ne2.DeltaShiftedCostFunction(this->current_state, mv);
  if (!shifted)
    mv_cost.shifted_value = (double)mv_cost.actual_value;
  Move2 best_move = mv;
  ShiftedResult<CFtype> best_delta = mv_cost;
  do  // look for the best non prohibited move
    {   // (if all moves are prohibited, then get the best)
      // For efficency, ProhibitedMove is invoked only when strictly necessary
      if (LessThan(mv_cost.shifted_value,best_delta.shifted_value))
	{
	  if (!this->pm2.ProhibitedMove(this->current_state, mv, mv_cost.actual_value))
	    {
	      best_move = mv;
	      best_delta = mv_cost;
	      all_moves_tabu = false;
	    }
	  if (all_moves_tabu)
	    {
	      best_move = mv;
	      best_delta = mv_cost;
	    }
	}
      else if (all_moves_tabu && !this->pm2.ProhibitedMove(this->current_state, mv, mv_cost.actual_value))
	{ // even though it is not an improving move,
	  // this move is the actual best since it's the first non-tabu
	  best_move = mv;
	  best_delta = mv_cost;
	  all_moves_tabu = false;
	}
      this->ne2.NextMove(this->current_state, mv);
      mv_cost = this->ne2.DeltaShiftedCostFunction(this->current_state, mv);
      if (!shifted)
	mv_cost.shifted_value = (double)mv_cost.actual_value;
    }
  while (!this->ne2.LastMoveDone(this->current_state, mv));
	
  this->current_move2 = best_move;
  // in all cases the cost should not be the shifted one
  this->current_move_cost2 = this->ne2.DeltaCostFunction(this->current_state, best_move);
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
