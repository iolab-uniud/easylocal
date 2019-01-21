#pragma once

/** The First Improvement Tabu Search runner differs from the
 @ref TabuSearch runner only in the selection of the move. A random sampling of the neighborhood is performed.
 @ingroup Runners
 */

#include "runners/tabusearch.hh"

namespace EasyLocal
{
  namespace Core
  {
    template <class StateManager, class NeighborhoodExplorer>
    class SampleTabuSearch : public TabuSearch<StateManager, NeighborhoodExplorer>
    {
    public:
      UNPACK_MOVERUNNER_BASIC_TYPES()
      
      using TabuSearch<StateManager, NeighborhoodExplorer>::TabuSearch;
      
      ENABLE_RUNNER_CLONE()
      
    protected:
      void SelectMove(const Input& in) override
      {
        size_t sampled = 0;
        CostStructure aspiration = this->best_state_cost - this->current_state_cost;
        EvaluatedMove em = this->ne.RandomBest(in, *this->p_current_state, samples, sampled, [this, aspiration](const Move &mv, const CostStructure &move_cost) {
          for (auto li : *(this->tabu_list))
            if ((move_cost >= aspiration) && this->Inverse(li.move, mv))
              return false;
          return true;
        },
                                                                    this->weights);
        this->current_move = em;
        this->evaluations += sampled;
      }
      
      void InitializeParameters() override
      {
        TabuSearch<StateManager, NeighborhoodExplorer>::InitializeParameters();
        samples("samples", "Number of neighbors sampled", this->parameters);
      }
      
      Parameter<unsigned int> samples;
    };
  } // namespace Core
} // namespace EasyLocal
