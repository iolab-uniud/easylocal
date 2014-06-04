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
    template <class Input, class State, class Move, typename CFtype>
    class FirstDescent : public MoveRunner<Input,State,Move,CFtype>
    {
    public:
      FirstDescent(const Input& in,
      StateManager<Input,State,CFtype>& e_sm,
      NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
      std::string name);
    protected:
      void InitializeRun();
      bool StopCriterion();
      bool AcceptableMove();
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
    @param in a poiter to an input object
    */
    template <class Input, class State, class Move, typename CFtype>
    FirstDescent<Input,State,Move,CFtype>::FirstDescent(const Input& in,
    StateManager<Input,State,CFtype>& e_sm, NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
    std::string name)
      : MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name)
    {
    }

    /**
    Selects always the first improving move in the neighborhood.
    */
    template <class Input, class State, class Move, typename CFtype>
    void FirstDescent<Input,State,Move,CFtype>::SelectMove()
    {
      this->current_move_cost = this->ne.FirstImprovingMove(this->current_state, this->current_move);
    }

    /**
    Invokes the companion superclass method, and initializes the move cost
    at a negative value for fulfilling the stop criterion the first time
    */
    template <class Input, class State, class Move, typename CFtype>
    void FirstDescent<Input,State,Move,CFtype>::InitializeRun()
    {
      MoveRunner<Input,State,Move,CFtype>::InitializeRun();
      this->current_move_cost = -1; // needed for passing the first time
      // the StopCriterion test
    }

    /**
    The search is stopped when no (strictly) improving move has been found.
    */
    template <class Input, class State, class Move, typename CFtype>
    bool FirstDescent<Input,State,Move,CFtype>::StopCriterion()
      { return GreaterOrEqualThan<CFtype>(this->current_move_cost,0); }

    /**
    A move is accepted if it is an improving one.
    */
    template <class Input, class State, class Move, typename CFtype>
    bool FirstDescent<Input,State,Move,CFtype>::AcceptableMove()
      { return LessThan<CFtype>(this->current_move_cost,0); }
        
  }
}

#endif // _FIRST_DESCENT_HH_
