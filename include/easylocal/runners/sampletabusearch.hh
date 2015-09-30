#if !defined(_SAMPLE_TABU_SEARCH_HH_)
#define _SAMPLE_TABU_SEARCH_HH_

/** The First Improvement Tabu Search runner differs from the
 @ref TabuSearch runner only in the selection of the move. A random sampling of the neighborhood is performed.
 @ingroup Runners
 */

#include "easylocal/runners/tabusearch.hh"

namespace EasyLocal {
  
  namespace Core {
    
    template <class Input, class State, class Move, typename CFtype = int, class CostStructure = DefaultCostStructure<CFtype>>
    class SampleTabuSearch : public TabuSearch<Input, State, Move, CFtype, CostStructure>
    {
    public:      
      using TabuSearch<Input, State, Move, CFtype, CostStructure>::TabuSearch;
      
      void RegisterParameters()
      {
        TabuSearch<Input, State, Move, CFtype, CostStructure>::RegisterParameters();
        samples("samples", "Number of neighbors sampled", this->parameters);
      }
    protected:
      void SelectMove();
      Parameter<unsigned int> samples;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Selects always the best move that is non prohibited by the tabu list
     mechanism.
     */
    template <class Input, class State, class Move, typename CFtype, class CostStructure>
    void SampleTabuSearch<Input, State, Move, CFtype, CostStructure>::SelectMove()
    {
      size_t sampled = 0;
      CostStructure aspiration = this->best_state_cost - this->current_state_cost;
      EvaluatedMove<Move, CFtype, CostStructure> em = this->ne.RandomBest(*this->p_current_state, samples, sampled, [this, aspiration](const Move& mv, const CostStructure& move_cost) {
        for (auto li : *(this->tabu_list))
          if ((move_cost >= aspiration) && this->Inverse(li.move, mv))
            return false;
        return true;
      }, this->weights);
      this->current_move = em;
      this->evaluations += sampled;
    }
  }
}

#endif // _SAMPLE_TABU_SEARCH_HH_
