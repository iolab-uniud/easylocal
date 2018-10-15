#pragma once

/** The First Improvement Tabu Search runner differs from the
 @ref TabuSearch runner only in the selection of the move. A random sampling of the neighborhood is performed.
 @ingroup Runners
 */

#include "easylocal/runners/tabusearch.hh"

namespace EasyLocal
{
  
  namespace Core
  {
    
    template <class Input, class State, class Move, class CostStructure = DefaultCostStructure<int>>
    class SampleTabuSearch : public TabuSearch<Input, State, Move, CostStructure>
    {
    public:
      using TabuSearch<Input, State, Move, CostStructure>::TabuSearch;
      
      void InitializeParameters()
      {
        TabuSearch<Input, State, Move, CostStructure>::InitializeParameters();
        samples("samples", "Number of neighbors sampled", this->parameters);
      }
      
    protected:
      void SelectMove(const Input& in);
      Parameter<unsigned int> samples;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Selects always the best move that is non prohibited by the tabu list
     mechanism.
     */
    template <class Input, class State, class Move, class CostStructure>
    void SampleTabuSearch<Input, State, Move, CostStructure>::SelectMove(const Input& in)
    {
      size_t sampled = 0;
      CostStructure aspiration = this->best_state_cost - this->current_state_cost;
      EvaluatedMove<Move, CostStructure> em = this->ne.RandomBest(in, *this->p_current_state, samples, sampled, [this, aspiration](const Move &mv, const CostStructure &move_cost) {
        for (auto li : *(this->tabu_list))
          if ((move_cost >= aspiration) && this->Inverse(li.move, mv))
            return false;
        return true;
      },
                                                                  this->weights);
      this->current_move = em;
      this->evaluations += sampled;
    }
  } // namespace Core
} // namespace EasyLocal
