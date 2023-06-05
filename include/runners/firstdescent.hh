#pragma once

#include "runners/moverunner.hh"
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
    template <class Input, class Solution, class Move, class CostStructure = DefaultCostStructure<int>>
    class FirstDescent : public MoveRunner<Input, Solution, Move, CostStructure>
    {
    public:
      using MoveRunner<Input, Solution, Move, CostStructure>::MoveRunner;
      
    protected:
      void InitializeRun();
      bool StopCriterion();
      void SelectMove();
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class Solution, class Move, class CostStructure>
    void FirstDescent<Input, Solution, Move, CostStructure>::InitializeRun()
    {
      MoveRunner<Input, Solution, Move, CostStructure>::InitializeRun();
      try
      {
        this->ne.FirstMove(*this->p_current_state, this->current_move.move);
      }
      catch (EmptyNeighborhood& e)
      {
        // FIXME: do something smart
      }
    }
    
    /**
     Selects always the first improving move in the neighborhood.
     */
    template <class Input, class Solution, class Move, class CostStructure>
    void FirstDescent<Input, Solution, Move, CostStructure>::SelectMove()
    {
      size_t explored;
      EvaluatedMove<Move, CostStructure> em = this->ne.SelectRandomFirst(*this->p_current_state, explored, 
                                                                   [](const Move &mv, const CostStructure &move_cost) {
                                                                     return move_cost < 0;
                                                                   },
                                                                   this->weights);
      this->current_move = em;
      this->evaluations += explored;
#if VERBOSE >= 2
      std::cerr << "V2 " << this->current_move.move << " (" << this->current_move.cost << ") " << std::endl;
#endif
    }
    
    /**
     The search is stopped when no (strictly) improving move has been found.
     */
    template <class Input, class Solution, class Move, class CostStructure>
    bool FirstDescent<Input, Solution, Move, CostStructure>::StopCriterion()
    {
      return this->iteration > 0 && !this->current_move.is_valid;
    }
  } // namespace Core
} // namespace EasyLocal
