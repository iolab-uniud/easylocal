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
    template <class Input, class State, class Move, typename CFtype = int>
    class SteepestDescent : public MoveRunner<Input, State, Move, CFtype>
    {
    public:
      
      SteepestDescent(const Input& in,
                      StateManager<Input, State, CFtype>& e_sm,
                      NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
                      std::string name);
      
    protected:
      void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
      void StoreMove();
      bool StopCriterion();
      void SelectMove();
      void NoAcceptableMoveFound();
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
    template <class Input, class State, class Move, typename CFtype>
    SteepestDescent<Input, State, Move, CFtype>::SteepestDescent(const Input& in,
                                                                 StateManager<Input, State, CFtype>& e_sm, NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
                                                                 std::string name)
    : MoveRunner<Input, State, Move, CFtype>(in, e_sm, e_ne, name, "Steepest Descent Runner")
    {}
    
    /**
     Selects always the best move in the neighborhood.
     */
    template <class Input, class State, class Move, typename CFtype>
    void SteepestDescent<Input, State, Move, CFtype>::SelectMove()
    {
      // TODO: perhaps the SelectBest method could already compute the different cost components and return all of them
      this->current_move_cost = this->ne.SelectBest(*this->p_current_state, this->current_move, [](const Move& mv, CFtype move_cost) {
        return LessThan(move_cost, (CFtype)0);
      });
      this->current_move_violations = this->ne.DeltaViolations(*this->p_current_state, this->current_move); // TODO: slightly inefficient
    }
    
    /**
     Invokes the companion superclass method, and initializes the move cost
     at a negative value for fulfilling the stop criterion the first time
     */
    template <class Input, class State, class Move, typename CFtype>
    void SteepestDescent<Input, State, Move, CFtype>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
    {
      MoveRunner<Input, State, Move, CFtype>::InitializeRun();
      this->current_move_cost = -1; // needed for passing the first time
                                    // the StopCriterion test
    }
    
    /**
     The search is stopped when no (strictly) improving move has been found.
     */
    template <class Input, class State, class Move, typename CFtype>
    bool SteepestDescent<Input, State, Move, CFtype>::StopCriterion()
    {
      return this->no_acceptable_move_found;
    }
  }
}

#endif // _STEEPEST_DESCENT_HH_
