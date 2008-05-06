#ifndef TABUSEARCHWITHSHIFTINGPENALTY_HH_
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
  Move mv;
  ShiftedResult<CFtype> mv_cost;
  bool all_moves_tabu = true;
	
  this->ne.FirstMove(this->current_state, mv);
  mv_cost = this->ne.DeltaShiftedCostFunction(this->current_state, mv);
  if (!shifted)
    mv_cost.shifted_value = (double)mv_cost.actual_value;

  Move best_shifted_move = mv, best_actual_move = mv;
  ShiftedResult<CFtype> best_shifted_delta = mv_cost;
  CFtype best_actual_delta = mv_cost.actual_value;
  do  // look for the best non prohibited move
    {   // (if all moves are prohibited, then get the best)
      // For efficency, ProhibitedMove is invoked only when strictly necessary
      if (LessThan(mv_cost.shifted_value, best_shifted_delta.shifted_value))
	{
	  if (!this->pm.ProhibitedMove(this->current_state, mv, mv_cost.actual_value))
	    {
	      best_shifted_move = mv;
	      best_shifted_delta = mv_cost;
	      all_moves_tabu = false;
	    }
	  if (all_moves_tabu)
	    {
	      best_shifted_move = mv;
	      best_shifted_delta = mv_cost;
	    }
	}
      else if (all_moves_tabu && !this->pm.ProhibitedMove(this->current_state, mv, mv_cost.actual_value))
	{ // even though it is not an improving move,
	  // this move is the actual best since it's the first non-tabu
	  best_shifted_move = mv;
	  best_shifted_delta = mv_cost;
	  all_moves_tabu = false;
	}
      /* Search for the best actual move as a sort of "Aspiration" for the case it improves over the current best */
      if (LessThan(mv_cost.actual_value, best_actual_delta))
	{
	  best_actual_move = mv;
	  best_actual_delta = mv_cost.actual_value;
	}
      this->ne.NextMove(this->current_state, mv);
      mv_cost = this->ne.DeltaShiftedCostFunction(this->current_state, mv);
      if (!shifted)
	mv_cost.shifted_value = (double)mv_cost.actual_value;
    }
  while (!this->ne.LastMoveDone(this->current_state, mv));

  if (LessThan(this->current_state_cost + best_actual_delta, this->best_state_cost))
    {
      this->current_move = best_actual_move;
      this->current_move_cost = best_actual_delta;
    }
  else
    {
      this->current_move = best_shifted_move;
      // in all cases the cost should not be the shifted one
      this->current_move_cost = best_shifted_delta.actual_value;
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
