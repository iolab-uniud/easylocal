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
      SampleTabuSearch<Input, State, Move, CFtype>(const Input& in,
                                                   StateManager<Input, State, CFtype>& e_sm,
                                                   NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
                                                   TabuListManager<State, Move, CFtype>& e_tlm,
                                                   std::string name) : TabuSearch<Input, State, Move, CFtype>(in, e_sm, e_ne, e_tlm, name) {}
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
      EvaluatedMove<Move, CFtype> em = this->ne.RandomBest(*this->p_current_state, samples, sampled, [this](const Move& mv, CostComponents<CFtype> move_cost) {
        return !this->pm.ProhibitedMove(*this->p_current_state, mv, move_cost.total);
      }, this->weights);
      this->current_move = em;
    }
  }
}

#endif // _SAMPLE_TABU_SEARCH_HH_
