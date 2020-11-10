#pragma once

#include "runners/tabusearch.hh"

namespace EasyLocal
{
  
  namespace Core
  {
    
    /** The First Improvement Tabu Search runner differs from the
     @ref TabuSearch runner only in the selection of the move. The first non-prohibited move
     that improves the cost function is selected.
     @ingroup Runners
     */
    template <class Input, class Solution, class Move, class CostStructure = DefaultCostStructure<int>>
    class FirstImprovementTabuSearch : public TabuSearch<Input, Solution, Move, CostStructure>
    {
    public:
      typedef typename CostStructure::CFtype CFtype;
      
      using TabuSearch<Input, Solution, Move, CostStructure>::TabuSearch;
      
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
    template <class Input, class Solution, class Move, class CostStructure>
    void FirstImprovementTabuSearch<Input, Solution, Move, CostStructure>::SelectMove()
    {
      CFtype aspiration = this->best_state_cost.total - this->current_state_cost.total;
      size_t explored;
      EvaluatedMove<Move, CostStructure> em = this->ne.SelectFirst(*this->p_current_state, explored, [this, aspiration](const Move &mv, const CostStructure &move_cost) {
        for (auto li : *(this->tabu_list))
          if ((move_cost.total >= aspiration) && this->Inverse(li.move, mv))
            return false;
        return true;
      },
                                                                   this->weights);
      this->current_move = em;
      this->evaluations += explored;
    }
  } // namespace Core
} // namespace EasyLocal
