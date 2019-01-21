#pragma once

#include "runners/moverunner.hh"

namespace EasyLocal
{  
  namespace Core
  {
    /** The First Descent runner performs a simple local search.
     At each step of the search, the first improving move in the neighborhood of current
     solution is selected and performed.
     @ingroup Runners
     */
    template <class StateManager, class NeighborhoodExplorer>
    class FirstDescent : public MoveRunner<StateManager, NeighborhoodExplorer>
    {
    public:
      UNPACK_MOVERUNNER_BASIC_TYPES()
      
      using MoveRunner<StateManager, NeighborhoodExplorer>::MoveRunner;
      
      ENABLE_RUNNER_CLONE()
      
    protected:
      void SelectMove(const Input& in) override final
      {
        size_t explored;
        EvaluatedMove em = this->ne.SelectFirst(in, *this->p_current_state, explored, [](const Move &mv, const CostStructure &move_cost) {
          return move_cost < 0;
        },
                                                this->weights);
        this->current_move = em;
        this->evaluations += explored;
      }

      bool StopCriterion() const override final
      {
        return this->iteration > 0 && !this->current_move.is_valid;
      }
    };
  } // namespace Core
} // namespace EasyLocal
