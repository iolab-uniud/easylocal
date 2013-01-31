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
  
  void Print(std::ostream& os = std::cout);
protected:
  
  void SelectMove();
  
  void MakeMove();
  
  void InitializeRun();

  void CompleteMove();

  
  // parameters
  Parameter<double> shift_region, alpha;
  Parameter<unsigned int> iterations_for_shift_update;
  // state of the shifting penalty
  std::vector<double> shifts;
  std::vector<CFtype> current_state_cost_components, current_move_cost_components;
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
shift_region("shift_region", "Shifting penalty region", this->parameters), alpha("alpha", "Shift adjustment", this->parameters),
iterations_for_shift_update("iterations_for_shift_update", "Number of iterations between shift updates", this->parameters)
{
  shifts.resize(this->ne.DeltaCostComponents(), 1.0);
  alpha = 2.0;
  iterations_for_shift_update = 1;
  current_state_hard_cost_components.resize(this->ne.DeltaCostComponents(), (CFtype)0);
  current_move_hard_cost_components.resize(this->ne.DeltaCostComponents(), (CFtype)0);
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearchWithShiftingPenalty<Input,State,Move,CFtype>::InitializeRun()
{
  TabuSearch<Input,State,Move,CFtype>::InitializeRun();
  // reset shifts
  for (double& s: shifts)
    s = 1.0;
  for (unsigned int i = 0; i < this->ne.DeltaCostComponents(); i++)
    current_state_hard_cost_components[i] = this->ne.DeltaCostComponents(i).GetCostComponent().CostFunction(*this->current_state);
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearchWithShiftingPenalty<Input,State,Move,CFtype>::SelectMove()
{
  bool shifted = (this->number_of_iterations - this->iteration_of_best < shift_region * this->max_idle_iterations);
  if (!shifted)
  {
    TabuSearch<Input, State, Move, CFtype>::SelectMove();
    current_move_cost_components = this->ne.DeltaCostFunctionComponents(*this->current_state, this->current_move);
    return;
  }
  // get the best non-prohibited move according to the shifted cost, but if all moves are prohibited, then get the best one among them
  unsigned int number_of_bests = 1, number_of_shifted_bests = 1; // number of moves found with the same best value
  const State& current_state = *this->p_current_state; // an alias for the current state object referenced through a pointer
  Move mv;
  this->ne.FirstMove(current_state, mv);
  CFtype mv_cost = (CFtype)0, shifted_mv_cost = (CFtype)0;
  std::vector<CFtype> mv_cost_components = this->ne.DeltaCostFunctionComponents(current_state, mv);
  for (unsigned int i = 0; i < this->ne.DeltaCostComponents(); i++)
  {
    mv_cost += (this->ne.DeltaCostComponents(i).IsHard() ? HARD_WEIGHT : 1) * mv_cost_components[i];
    shifted_mv_cost += (this->ne.DeltaCostComponents(i).IsHard() ? HARD_WEIGHT : 1) * shifts[i] * mv_cost_components[i];
  }
  Move best_move = mv, best_shifted_move = mv;
  CFtype best_delta = mv_cost, best_shifted_delta = shifted_mv_cost;
  bool all_moves_prohibited = pm.ProhibitedMove(current_state, mv, mv_cost);
  CFtype current_best_state_cost = this->best_state_cost;
  
  while (this->ne.NextMove(current_state, mv))
  {
    mv_cost = (CFtype)0;
    shifted_mv_cost = (CFtype)0;
    mv_cost_components = this->ne.DeltaCostFunctionComponents(current_state, mv);
    for (unsigned int i = 0; i < this->ne.DeltaCostComponents(); i++)
    {
      mv_cost += (this->ne.DeltaCostComponents(i).IsHard() ? HARD_WEIGHT : 1) * mv_cost_components[i];
      shifted_mv_cost += (this->ne.DeltaCostComponents(i).IsHard() ? HARD_WEIGHT : 1) * shifts[i] * mv_cost_components[i];
    }
    if (LessThan(mv_cost, best_delta) || LessThan(shifted_mv_cost, shifted_best_delta))
    {
      if (!pm.ProhibitedMove(current_state, mv, mv_cost))
      {
        if (LessThan(mv_cost, best_delta))
        {
          best_move = mv;
          best_delta = mv_cost;
          number_of_bests = 1;
        }
        if (LessThan(shifted_mv_cost, shifted_best_delta))
        {
          best_shifted_move = mv;
          best_shifted_delta = mv_cost;
          number_of_shifted_bests = 1;
        }
        all_moves_prohibited = false;
      }
      else if (all_moves_prohibited)
      {
        if (LessThan(mv_cost, best_delta))
        {
          best_move = mv;
          best_delta = mv_cost;
          number_of_bests = 1;
        }
        if (LessThan(shifted_mv_cost, shifted_best_delta))
        {
          best_shifted_move = mv;
          best_shifted_delta = mv_cost;
          number_of_shifted_bests = 1;
        }
      }
    }
    else if (EqualTo(mv_cost, best_delta) || EqualTo(shifted_mv_cost, shifted_best_delta))
    {
      if (!pm.ProhibitedMove(current_state, mv, mv_cost))
      {
        if (all_moves_prohibited)
	      {
          if (EqualTo(mv_cost, best_delta))
          {
            best_move = mv;
            number_of_bests = 1;
          }
          if (EqualTo(shifted_mv_cost, shifted_best_delta))
          {
            shifted_best_move = mv;
            number_of_shifted_bests = 1;
          }
          all_moves_prohibited = false;
	      }
        else
	      {
          if (EqualTo(mv_cost, best_delta))
          {
            if (Random::Int(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
              best_move = mv;
            number_of_bests++;
          }
          if (EqualTo(shifted_mv_cost, shifted_best_delta))
          {
            if (Random::Int(0, shifted_number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
              shifted_best_move = mv;
            shifted_number_of_bests++;
          }
	      }
      }
      else
        if (all_moves_prohibited)
        {
          if (EqualTo(mv_cost, best_delta))
          {
            if (Random::Int(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
              best_move = mv;
            number_of_bests++;
          }
          if (EqualTo(shifted_mv_cost, shifted_best_delta))
          {
            if (Random::Int(0, shifted_number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
              shifted_best_move = mv;
            shifted_number_of_bests++;
          }
        }
    }
    else // mv_cost is greater than best_delta
      if (all_moves_prohibited && !pm.ProhibitedMove(current_state, mv, mv_cost))
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
        shifted_best_move = mv;
        shifted_best_delta = shifted_mv_cost;
        shifted_number_of_bests = 1;
        all_moves_prohibited = false;
      }    
  }
  
  // "aspiration criterion" for the shifting penalty
  
  if (this->current_state_cost + best_delta < this->best_state_cost)
  {
    this->current_move = best_move;
    this->current_move_cost = best_delta;
  }
  else
  {
    this->current_move = shifted_best_move;
    this->current_move_cost = this->ne.DeltaCostFunction(current_state, this->current_move);
    current_move_hard_delta_cost_components = this->ne.DeltaCostFunctionComponents(current_state, this->current_move);
  }
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearchWithShiftingPenalty<Input,State,Move,CFtype>::MakeMove()
{
  TabuSearch<Input,Stte,Move,CFtype>::MakeMove();
  // update the hard components of the cost function
  for (unsigned int i = 0; i < this->ne.DeltaHardCostComponents(); i++)
    current_state_hard_cost_components[i] += current_move_hard_delta_cost_components[i];
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearchWithShiftingPenalty<Input,State,Move,CFtype>::CompleteMove()
{
  if (LessThan(this->current_state_cost, this->best_state_cost))
  {
    // reset shifts
    for (double& s: shifts)
      s = 1.0;
  }
  else if (this->number_of_iterations % iterations_for_shift_update == 0 && this->number_of_iterations - this->iteration_of_best < shift_region * this->max_idle_iteration)
  {
    // update shifts
    for (unsigned int i = 0; i < this->ne.DeltaCostComponents(); i++)
    {
      if (current_state_cost_components[i] == 0)
        shifts[i] /= Random::Double(0.95, 1.05) * alpha;
      else
        shifts[i] *= Random::Double(0.95, 1.05) * alpha;
    }
  }
  else
  {
    // reset shifts (out of the shift region)
    for (double& s: shifts)
      s = 1.0;
  }
  TabuSearch<Input, State, Move, CFtype>::CompleteMove();
}

#endif // _TABU_SEARCH_WITH_SHIFTING_PENALTY_HH_
