#if !defined(_FIRST_DESCENT_HH_)
#define _FIRST_DESCENT_HH_

#include "easylocal/runners/moverunner.hh"
#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** The First Descent runner performs a simple local search.
     At each step of the search, the first improving move in the neighborhood of current
     solution is selected and performed.
     @ingroup Runners
     */
    template <class Input, class State, class Move, class CostStructure = DefaultCostStructure<int>>
    class FirstDescent : public MoveRunner<Input, State, Move, CostStructure>
    {
    public:
      FirstDescent(const Input& in,
                   StateManager<Input, State, CostStructure>& e_sm,
                   NeighborhoodExplorer<Input, State, Move, CostStructure>& e_ne,
                   std::string name);
    protected:
      bool StopCriterion();
      void SelectMove();
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Constructs a first descent runner by linking it to a state manager,
     a neighborhood explorer, and an input object.
     
     @param s a pointer to a compatible state manager
     @param ne a pointer to a compatible neighborhood explorer
     @param in a pointer to an input object
     */
    template <class Input, class State, class Move, class CostStructure>
    FirstDescent<Input, State, Move, CostStructure>::FirstDescent(const Input& in,
                                                           StateManager<Input, State, CostStructure>& e_sm, NeighborhoodExplorer<Input, State, Move, CostStructure>& e_ne,
                                                           std::string name)
    : MoveRunner<Input, State, Move, CostStructure>(in, e_sm, e_ne, name, "First Descent Runner")
    {}
    
    /**
     Selects always the first improving move in the neighborhood.
     */
    template <class Input, class State, class Move, class CostStructure>
    void FirstDescent<Input, State, Move, CostStructure>::SelectMove()
    {
      size_t explored;
      EvaluatedMove<Move, CostStructure> em = this->ne.SelectFirst(*this->p_current_state, explored, [](const Move& mv, const CostStructure& move_cost) {
        return move_cost < 0;
      }, this->weights);
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
  }
}

#endif // _FIRST_DESCENT_HH_
