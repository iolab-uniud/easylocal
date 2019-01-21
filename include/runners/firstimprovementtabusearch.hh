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
    template <class StateManager, class NeighborhoodExplorer>
    class FirstImprovementTabuSearch : public TabuSearch<StateManager, NeighborhoodExplorer>
    {
    public:
      UNPACK_MOVERUNNER_BASIC_TYPES()

      using TabuSearch<StateManager, NeighborhoodExplorer>::TabuSearch;
      
      ENABLE_RUNNER_CLONE()
      
    protected:
      void SelectMove(const Input& in) override
      {
        CostStructure aspiration = this->best_state_cost - this->current_state_cost;
        size_t explored;
        EvaluatedMove em = this->ne.SelectFirst(in, *this->p_current_state, explored, [this, in, aspiration](const Move &mv, const CostStructure &move_cost) {
          for (auto li : *(this->tabu_list))
            if ((move_cost >= aspiration) && this->Inverse(in, *this->p_current_state, li.move, mv))
              return false;
          return true;
        },
                                                this->weights);
        this->current_move = em;
        this->evaluations += explored;
      }
    };
  } // namespace Core
} // namespace EasyLocal
