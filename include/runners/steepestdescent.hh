#pragma once

#include "runners/moverunner.hh"

namespace EasyLocal
{
  
  namespace Core
  {    
    /** The Steepest Descent runner performs a simple local search.
     At each step of the search, the best move in the neighborhood of current
     solution is selected and performed.
     It is worth noticing that this algorithm leads straightly to the
     nearest local minimum of a given state.
     @ingroup Runners
     */
    template <class StateManager, class NeighborhoodExplorer>
    class SteepestDescent : public MoveRunner<StateManager, NeighborhoodExplorer>
    {
    public:
      UNPACK_MOVERUNNER_BASIC_TYPES()
      
      using MoveRunner<StateManager, NeighborhoodExplorer>::MoveRunner;
      
      ENABLE_RUNNER_CLONE()
      
    protected:
      /**
       Selects always the best move in the neighborhood.
       */
      void SelectMove(const Input& in) override final
      {
        size_t explored;
        EvaluatedMove em = this->ne.SelectBest(in, *this->p_current_state, explored, [](const Move &mv, const CostStructure &move_cost) {
          return move_cost < 0;
        },
                                                                     this->weights);
        this->current_move = em;
        this->evaluations += static_cast<unsigned long int>(explored);
      }
      
      /**
       The search is stopped when no (strictly) improving move has been found.
       */
      bool StopCriterion() const override final
      {
        return this->iteration > 0 && (!this->current_move.is_valid || this->current_move.cost <= 0);
      }
      

    };
  } // namespace Core
} // namespace EasyLocal
