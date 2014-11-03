#if !defined(_FIRST_IMPROVEMENT_TABU_SEARCH_HH_)
#define _FIRST_IMPROVEMENT_TABU_SEARCH_HH_

#include "easylocal/runners/tabusearch.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** The First Improvement Tabu Search runner differs from the
     @ref TabuSearch runner only in the selection of the move. The first non-prohibited move
     that improves the cost function is selected.
     @ingroup Runners
     */
    template <class Input, class State, class Move, typename CFtype = int>
    class FirstImprovementTabuSearch : public TabuSearch<Input, State, Move, CFtype>
    {
    public:
      using TabuSearch<Input, State, Move, CFtype>::TabuSearch;
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
    void FirstImprovementTabuSearch<Input, State, Move, CFtype>::SelectMove()
    {
      EvaluatedMove<Move, CFtype> em = this->ne.SelectFirst(*this->p_current_state, [this](const Move& mv, CostComponents<CFtype> move_cost) {
        return !this->pm.ProhibitedMove(*this->p_current_state, mv, move_cost.total);
      }, this->weights);
      this->current_move = em;
    }
  }
}

#endif // _FIRST_IMPROVEMENT_TABU_SEARCH_HH_
