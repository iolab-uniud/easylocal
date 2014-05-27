#if !defined(_FIRST_IMPROVEMENT_TABU_SEARCH_HH_)
#define _FIRST_IMPROVEMENT_TABU_SEARCH_HH_

/** The First Improvement Tabu Search runner differs from the
  @ref TabuSearch runner only in the selection of the move. The first non-prohibited move
  that improves the cost function is selected.
 @ingroup Runners
 */

#include "TabuSearch.hh"

template <class Input, class State, class Move, typename CFtype>
class FirstImprovementTabuSearch : public TabuSearch<Input,State,Move,CFtype>
{
public:
  FirstImprovementTabuSearch(const Input& in, StateManager<Input,State,CFtype>& e_sm,
                                                       NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                       TabuListManager<State,Move,CFtype>& e_tlm,
                                                       std::string name) : TabuSearch<Input,State,Move,CFtype>(in, e_sm, e_ne, e_tlm, name) {}
protected:
  void SelectMove();
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
 Selects always the best move that is non prohibited by the tabu list 
 mechanism.
 */
template <class Input, class State, class Move, typename CFtype>
void FirstImprovementTabuSearch<Input,State,Move,CFtype>::SelectMove()
{
	// get the first non-prohibited move, but if all moves are prohibited, then get the best one among them
  unsigned int number_of_bests = 0;
	const State& current_state = *this->p_current_state; // an alias for the current state
  Move mv;
  this->ne.FirstMove(current_state, mv);
  CFtype mv_cost = this->ne.DeltaCostFunction(current_state, mv);
  Move best_move = mv;
  CFtype best_delta = mv_cost;
  bool all_moves_prohibited = true, not_last_move;
  
  do // look for the best move
  {  // if the prohibition mechanism is active get the best non-prohibited move
		// if all moves are prohibited, then get the best one
		if (LessThan(mv_cost, (CFtype)0) && !this->pm.ProhibitedMove(current_state, mv, mv_cost))
		{
			// mv is an improving move
		  this->current_move = mv;
		  this->current_move_cost = mv_cost;
      return;
		}
		
    if (LessThan(mv_cost, best_delta))
    {
      if (!this->pm.ProhibitedMove(current_state, mv, mv_cost))
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
        all_moves_prohibited = false;
      }
      if (all_moves_prohibited)
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
      }
    }
    else if (all_moves_prohibited && !this->pm.ProhibitedMove(current_state, mv, mv_cost))
    { // when the prohibition mechanism is active, even though it is not an improving move,
      // this move is the actual best since it is the first non-prohibited
      best_move = mv;
      best_delta = mv_cost;
      number_of_bests = 1;
      all_moves_prohibited = false;
    }
    else if (EqualTo(mv_cost, best_delta) && !this->pm.ProhibitedMove(current_state, mv, mv_cost))
    {
      if (Random::Int(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
        best_move = mv;
      number_of_bests++;
    }
    not_last_move = this->ne.NextMove(current_state, mv);
    if (not_last_move)
      mv_cost = this->ne.DeltaCostFunction(current_state, mv);
  }
  while (not_last_move);
    
  this->current_move = best_move;
  this->current_move_cost = best_delta;
}

#endif // _FIRST_IMPROVEMENT_TABU_SEARCH_HH_
