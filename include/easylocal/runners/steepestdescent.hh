#if !defined(_STEEPEST_DESCENT_HH_)
#define _STEEPEST_DESCENT_HH_

#include "easylocal/runners/moverunner.hh"
#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"
#include "easylocal/utils/types.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** The Steepest Descent runner performs a simple local search.
     At each step of the search, the best move in the neighborhood of current
     solution is selected and performed.
     It is worth noticing that this algorithm leads straightly to the
     nearest local minimum of a given state.
     @ingroup Runners
     */
    template <class Input, class State, class Move, typename CFtype = int, class CostStructure = DefaultCostStructure<CFtype>>
    class SteepestDescent : public MoveRunner<Input, State, Move, CFtype, CostStructure>
    {
    public:
      
      SteepestDescent(const Input& in,
                      StateManager<Input, State, CFtype, CostStructure>& e_sm,
                      NeighborhoodExplorer<Input, State, Move, CFtype, CostStructure>& e_ne,
                      std::string name);
      
    protected:
      void StoreMove();
      bool StopCriterion();
      void SelectMove();
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Constructs a steepest descent runner by linking it to a state manager,
     a neighborhood explorer, and an input object.
     
     @param s a pointer to a compatible state manager
     @param ne a pointer to a compatible neighborhood explorer
     @param in a pointer to an input object
     */
    template <class Input, class State, class Move, typename CFtype, class CostStructure>
    SteepestDescent<Input, State, Move, CFtype, CostStructure>::SteepestDescent(const Input& in,
                                                                 StateManager<Input, State, CFtype, CostStructure>& e_sm, NeighborhoodExplorer<Input, State, Move, CFtype, CostStructure>& e_ne,
                                                                 std::string name)
    : MoveRunner<Input, State, Move, CFtype>(in, e_sm, e_ne, name, "Steepest Descent Runner")
    {}
    
    /**
     Selects always the best move in the neighborhood.
     */
    template <class Input, class State, class Move, typename CFtype, class CostStructure>
    void SteepestDescent<Input, State, Move, CFtype, CostStructure>::SelectMove()
    {
      size_t explored;
      EvaluatedMove<Move, CFtype, CostStructure> em = this->ne.SelectBest(*this->p_current_state, explored, [](const Move& mv, const CostStructure& move_cost) {
        return move_cost < 0;
      }, this->weights);
      this->current_move = em;
      this->evaluations += explored;
    }
    
    /**
     The search is stopped when no (strictly) improving move has been found.
     */
    template <class Input, class State, class Move, typename CFtype, class CostStructure>
    bool SteepestDescent<Input, State, Move, CFtype, CostStructure>::StopCriterion()
    {
      return this->iteration > 0 && !this->current_move.is_valid;
    }
  }
}

#endif // _STEEPEST_DESCENT_HH_
