#if !defined(_SAMPLE_TABU_SEARCH_HH_)
#define _SAMPLE_TABU_SEARCH_HH_

/** The First Improvement Tabu Search runner differs from the
 @ref TabuSearch runner only in the selection of the move. A random sampling of the neighborhood is performed.
 @ingroup Runners
 */

#include "easylocal/runners/tabusearch.hh"

namespace EasyLocal {
  
  namespace Core {
    
    template <class Input, class State, class Move, typename CFtype = int>
    class SampleTabuSearch : public TabuSearch<Input, State, Move, CFtype>
    {
    public:
      SampleTabuSearch<Input, State, Move, CFtype>(const Input& in,
                                                   StateManager<Input, State, CFtype>& e_sm,
                                                   NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
                                                   TabuListManager<State, Move, CFtype>& e_tlm,
                                                   std::string name) : TabuSearch<Input, State, Move, CFtype>(in, e_sm, e_ne, e_tlm, name), samples("samples", "Number of neighbors sampled", this->parameters) {}
    protected:
      void SelectMove();
      Parameter<unsigned int> samples;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Selects always the best move that is non prohibited by the tabu list
     mechanism.
     */
    template <class Input, class State, class Move, typename CFtype>
    void SampleTabuSearch<Input, State, Move, CFtype>::SelectMove()
    {
      // get the first non-prohibited move
      unsigned int number_of_bests = 0;
      unsigned int s = 1;
      const State& current_state = *this->p_current_state;
      Move mv;
      CFtype mv_cost;
      bool all_moves_prohibited = true;
      
      this->ne.RandomMove(current_state, mv);
      mv_cost = this->ne.DeltaCostFunction(current_state, mv);
      Move best_move = mv;
      CFtype best_delta = mv_cost;
      do
      {
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
        this->ne.RandomMove(current_state, mv);
        mv_cost = this->ne.DeltaCostFunction(current_state, mv);
        s++;
      }
      while (s < samples);
      
      this->current_move = best_move;
      this->current_move_cost = best_delta;
    }
  }
}

#endif // _SAMPLE_TABU_SEARCH_HH_
