#pragma once

#include "runners/moverunner.hh"
#include "helpers/statemanager.hh"
#include "helpers/neighborhoodexplorer.hh"

namespace EasyLocal
{
  
  namespace Core
  {
    
    /** The First Descent runner performs a simple local search.
     At each step of the search, the first improving move in the neighborhood of current
     solution is selected and performed.
     @ingroup Runners
     */
    template <class Input, class State, class Move, class CostStructure = DefaultCostStructure<int>>
    class FirstDescent : public MoveRunner<Input, State, Move, CostStructure>
    {
    public:
      using MoveRunner<Input, State, Move, CostStructure>::MoveRunner;
      
    protected:
      bool StopCriterion();
      void SelectMove();
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Selects always the first improving move in the neighborhood.
     */
    template <class Input, class State, class Move, class CostStructure>
    void FirstDescent<Input, State, Move, CostStructure>::SelectMove()
    {
      size_t explored;
      EvaluatedMove<Move, CostStructure> em = this->ne.SelectFirst(*this->p_current_state, explored, [](const Move &mv, const CostStructure &move_cost) {
        return move_cost < 0;
      },
                                                                   this->weights);
      this->current_move = em;
      this->evaluations += explored;
    }
    
    /**
     The search is stopped when no (strictly) improving move has been found.
     */
    template <class Input, class State, class Move, class CostStructure>
    bool FirstDescent<Input, State, Move, CostStructure>::StopCriterion()
    {
      return this->iteration > 0 && !this->current_move.is_valid;
    }
  } // namespace Core
} // namespace EasyLocal
