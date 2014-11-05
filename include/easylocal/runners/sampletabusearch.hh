#if !defined(_SAMPLE_TABU_SEARCH_HH_)
#define _SAMPLE_TABU_SEARCH_HH_

/** The First Improvement Tabu Search runner differs from the
 @ref TabuSearch runner only in the selection of the move. A random sampling of the neighborhood is performed.
 @ingroup Runners
 */

#include "easylocal/runners/tabusearch.hh"

namespace EasyLocal {
  
  namespace Core {
    
    template <class Input, class State, class Move, typename CFtype = int>
    class SampleTabuSearch : public TabuSearch<Input, State, Move, CFtype>
    {
    public:      
      using TabuSearch<Input, State, Move, CFtype>::TabuSearch;
      
      void RegisterParameters()
      {
        TabuSearch<Input, State, Move, CFtype>::RegisterParameters();
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
    template <class Input, class State, class Move, typename CFtype>
    void SampleTabuSearch<Input, State, Move, CFtype>::SelectMove()
    {
      size_t sampled = 0;
      CFtype aspiration = this->best_state_cost.total - this->current_state_cost.total;
      EvaluatedMove<Move, CFtype> em = this->ne.RandomBest(*this->p_current_state, samples, sampled, [this, aspiration](const Move& mv, CostStructure<CFtype> move_cost) {
        for (auto li : *(this->tabu_list))
          if (this->Inverse(li.move, mv) && (move_cost.total >= aspiration))
            return false;
        return true;
      }, this->weights);
      this->current_move = em;
    }
  }
}

#endif // _SAMPLE_TABU_SEARCH_HH_
