#pragma once

#include "easylocal/runners/tabusearch.hh"

namespace EasyLocal
{
  
  namespace Core
  {
    
    /** The First Improvement Tabu Search runner differs from the
     @ref TabuSearch runner only in the selection of the move. The first non-prohibited move
     that improves the cost function is selected.
     @ingroup Runners
     */
    template <class Input, class State, class Move, class CostStructure = DefaultCostStructure<int>>
    class FirstImprovementTabuSearch : public TabuSearch<Input, State, Move, CostStructure>
    {
    public:
      typedef typename CostStructure::CFtype CFtype;
      
      using TabuSearch<Input, State, Move, CostStructure>::TabuSearch;
      
    protected:
      void SelectMove(const Input& in);
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Selects always the best move that is non prohibited by the tabu list
     mechanism.
     */
    template <class Input, class State, class Move, class CostStructure>
    void FirstImprovementTabuSearch<Input, State, Move, CostStructure>::SelectMove(const Input& in)
    {
      CFtype aspiration = this->best_state_cost.total - this->current_state_cost.total;
      size_t explored;
      EvaluatedMove<Move, CostStructure> em = this->ne.SelectFirst(in, *this->p_current_state, explored, [this, aspiration](const Move &mv, const CostStructure &move_cost) {
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
