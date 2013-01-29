#if !defined(_TABU_SEARCH_WITH_SHIFTING_PENALTY_HH_)
#define _TABU_SEARCH_WITH_SHIFTING_PENALTY_HH_

#include <runners/TabuSearch.hh>

template <class Input, class State, class Move, typename CFtype>
class TabuSearchWithShiftingPenalty : public TabuSearch<Input,State,Move,CFtype>
{
public:
  
  TabuSearchWithShiftingPenalty(const Input& in,
                                StateManager<Input,State,CFtype>& sm,
                                NeighborhoodExplorer<Input,State,Move,CFtype>& ne,
                                TabuListManager<State,Move,CFtype>& tlm,
                                std::string name);
  
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  void SetShiftRegion(double sr)
  { shift_region = sr; }
  void SetWeightRegion(double w) { shift_region = w; }


protected:
  
  void SelectMove();
  
  void InitializeRun();

  void CompleteMove();
  
  // for the shifting penalty
  void ResetShifts();
  void UpdateShifts();
  
  // parameters
  Parameter<double> shift_region;
  // state
  bool shifts_reset;
  std::vector<double> shifts;
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
// parameters
shift_region("shift_region", "Shifting penalty region", this->parameters)
{
  shifts.resize(this->ne.);
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearchWithShiftingPenalty<Input,State,Move,CFtype>::ResetShifts()
{
 
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
  bool shifted = (this->number_of_iterations - this->iteration_of_best < shift_region * this->max_idle_iterations);
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
void TabuSearchWithShiftingPenalty<Input,State,Move,CFtype>::CompleteMove()
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
  TabuSearch<Input,State,Move,CFtype>::CompleteMove();
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearchWithShiftingPenalty<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  for (unsigned i = 0; i < this->ne.DeltaCostComponents(); i++)
    this->ne.DeltaCostComponent(i).ReadParameters(is, os);
}

#endif // _TABU_SEARCH_WITH_SHIFTING_PENALTY_HH_
