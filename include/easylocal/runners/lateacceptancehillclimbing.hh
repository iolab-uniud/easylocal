#if !defined(_LATE_ACCEPTANCE_HILL_CLIMBING_HH_)
#define _LATE_ACCEPTANCE_HILL_CLIMBING_HH_

#include <stdexcept>

#include "easylocal/runners/hillclimbing.hh"
#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** The Late Acceptance Hill Climbing maintains a list of previous
     moves and defers acceptance to k steps further.
     
     @ingroup Runners
     */
    template <class Input, class State, class Move, typename CFtype = int, class Compare = std::less<CostStructure<CFtype>>>
    class LateAcceptanceHillClimbing : public HillClimbing<Input, State, Move, CFtype, Compare>
    {
    public:
      
      LateAcceptanceHillClimbing(const Input& in,
                                 StateManager<Input, State, CFtype>& e_sm,
                                 NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
                                 std::string name);
    protected:
      void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
      void CompleteMove();
      void SelectMove();
      
      // parameters
      void RegisterParameters();
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
    template <class Input, class State, class Move, typename CFtype, class Compare>
    LateAcceptanceHillClimbing<Input, State, Move, CFtype, Compare>::LateAcceptanceHillClimbing(const Input& in,
                                                                                       StateManager<Input, State, CFtype>& e_sm,
                                                                                       NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
                                                                                       std::string name)
    : HillClimbing<Input, State, Move, CFtype, Compare>(in, e_sm, e_ne, name)
    {}
    
    template <class Input, class State, class Move, typename CFtype, class Compare>
    void LateAcceptanceHillClimbing<Input, State, Move, CFtype, Compare>:: RegisterParameters()
    {
      HillClimbing<Input, State, Move, CFtype, Compare>::RegisterParameters();
      steps("steps", "Delay (number of steps in the queue)", this->parameters);
      steps = 10;
    }
    
    /**
     Initializes the run by invoking the companion superclass method, and
     setting the temperature to the start value.
     */
    template <class Input, class State, class Move, typename CFtype, class Compare>
    void LateAcceptanceHillClimbing<Input, State, Move, CFtype, Compare>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
    {
      HillClimbing<Input, State, Move, CFtype, Compare>::InitializeRun();
      
      // the queue must be filled with the initial state cost at the beginning
      previous_steps = std::vector<CFtype>(steps);
      std::fill(previous_steps.begin(), previous_steps.end(), this->current_state_cost.total);
    }
    
    
    /** A move is surely accepted if it improves the cost function
     or with exponentially decreasing probability if it is
     a worsening one.
     */
    template <class Input, class State, class Move, typename CFtype, class Compare>
    void LateAcceptanceHillClimbing<Input, State, Move, CFtype, Compare>::SelectMove()
    {
      // TODO: it should become a parameter, the number of neighbors drawn at each iteration (possibly evaluated in parallel)
      const size_t samples = 10;
      size_t sampled;
      CFtype prev_step_delta_cost = previous_steps[this->iteration % steps] - this->current_state_cost.total;
      // TODO: check shifting penalty meaningfullness
      EvaluatedMove<Move, CFtype> em = this->ne.RandomFirst(*this->p_current_state, samples, sampled, [prev_step_delta_cost](const Move& mv, CostStructure<CFtype> move_cost) {
        return move_cost <= 0 || move_cost <= prev_step_delta_cost;
      }, this->weights);
      this->current_move = em;
      this->evaluations += sampled;
    }
    
    /**
     A move is randomly picked.
     */
    template <class Input, class State, class Move, typename CFtype, class Compare>
    void LateAcceptanceHillClimbing<Input, State, Move, CFtype, Compare>::CompleteMove()
    {
      previous_steps[this->iteration % steps] = this->best_state_cost.total;
    }
  }
}

#endif // _LATE_ACCEPTANCE_HILL_CLIMBING_HH_

