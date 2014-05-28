#if !defined(_LATE_ACCEPTANCE_HILL_CLIMBING_HH_)
#define _LATE_ACCEPTANCE_HILL_CLIMBING_HH_

#include <stdexcept>

#include "runners/HillClimbing.hh"
#include "helpers/StateManager.hh"
#include "helpers/NeighborhoodExplorer.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** The Late Acceptance Hill Climbing maintains a list of previous
    moves and defers acceptance to k steps further.
 
    @ingroup Runners
    */
    template <class Input, class State, class Move, typename CFtype>
    class LateAcceptanceHillClimbing : public HillClimbing<Input,State,Move,CFtype>
    {
    public:
  
      LateAcceptanceHillClimbing(const Input& in,
      StateManager<Input,State,CFtype>& e_sm,
      NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
      std::string name);	
  
  
    protected:
      void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
      bool AcceptableMove();
      void CompleteMove();
  
      // parameters
      Parameter<unsigned int> steps;
      std::vector<CFtype> previous_steps;
  
    };

    /*************************************************************************
    * Implementation
    *************************************************************************/

    /**
    Constructs a simulated annealing runner by linking it to a state manager, 
    a neighborhood explorer, and an input object.
 
    @param s a pointer to a compatible state manager
    @param ne a pointer to a compatible neighborhood explorer
    @param in a poiter to an input object
    */
    template <class Input, class State, class Move, typename CFtype>
    LateAcceptanceHillClimbing<Input,State,Move,CFtype>::LateAcceptanceHillClimbing(const Input& in,
    StateManager<Input,State,CFtype>& e_sm,
    NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
    std::string name)
      : HillClimbing<Input,State,Move,CFtype>(in, e_sm, e_ne, name),
    steps("steps", "Delay (number of steps in the queue)", this->parameters)
    {
      steps = 10;
    }

    /**
    Initializes the run by invoking the companion superclass method, and
    setting the temperature to the start value.
    */
    template <class Input, class State, class Move, typename CFtype>
    void LateAcceptanceHillClimbing<Input,State,Move,CFtype>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
    {    
      HillClimbing<Input,State,Move,CFtype>::InitializeRun();
  
      // the queue must be filled with the initial state cost at the beginning
      previous_steps = std::vector<CFtype>(steps);
      fill(previous_steps.begin(), previous_steps.end(), this->current_state_cost);
    }


    /**
    A move is randomly picked.
    */
    template <class Input, class State, class Move, typename CFtype>
    void LateAcceptanceHillClimbing<Input,State,Move,CFtype>::CompleteMove()
    {
      previous_steps[this->iteration % steps] = this->best_state_cost;
    }

    /** A move is surely accepted if it improves the cost function
    or with exponentially decreasing probability if it is 
    a worsening one.
    */
    template <class Input, class State, class Move, typename CFtype>
    bool LateAcceptanceHillClimbing<Input,State,Move,CFtype>::AcceptableMove()
    {
      return LessOrEqualThan(this->current_move_cost,(CFtype)0)
        || LessOrEqualThan(this->current_move_cost + this->current_state_cost, previous_steps[this->iteration % steps]);
    }
  }
}

#endif // _LATE_ACCEPTANCE_HILL_CLIMBING_HH_

