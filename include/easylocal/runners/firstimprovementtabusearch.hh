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
    template <class Input, class State, class Move, typename CFtype = int, class Compare = std::less<CostStructure<CFtype>>>
    class FirstImprovementTabuSearch : public TabuSearch<Input, State, Move, CFtype, Compare>
    {
    public:
      using TabuSearch<Input, State, Move, CFtype, Compare>::TabuSearch;
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
    template <class Input, class State, class Move, typename CFtype, class Compare>
    void FirstImprovementTabuSearch<Input, State, Move, CFtype, Compare>::SelectMove()
    {
      CFtype aspiration = this->best_state_cost.total - this->current_state_cost.total;
      size_t explored;
      EvaluatedMove<Move, CFtype> em = this->ne.SelectFirst(*this->p_current_state, explored, [this, aspiration](const Move& mv, CostStructure<CFtype> move_cost) {
        for (auto li : *(this->tabu_list))
          if ((move_cost.total >= aspiration) && this->Inverse(li.move, mv))
            return false;
        return true;
      }, this->weights);
      this->current_move = em;
      this->evaluations += explored;
    }
  }
}

#endif // _FIRST_IMPROVEMENT_TABU_SEARCH_HH_
