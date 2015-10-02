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
    template <class Input, class State, class Move, class CostStructure = DefaultCostStructure<int>>
    class LateAcceptanceHillClimbing : public HillClimbing<Input, State, Move, CostStructure>
    {
    public:
      
      LateAcceptanceHillClimbing(const Input& in,
                                 StateManager<Input, State, CostStructure>& e_sm,
                                 NeighborhoodExplorer<Input, State, Move, CostStructure>& e_ne,
                                 std::string name);
    protected:
      void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
      void CompleteMove();
      void SelectMove();
      
      // parameters
      void RegisterParameters();
      Parameter<unsigned int> steps;
      std::vector<CostStructure> previous_steps;
      
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Constructs a simulated annealing runner by linking it to a state manager,
     a neighborhood explorer, and an input object.
     
     @param s a pointer to a compatible state manager
     @param ne a pointer to a compatible neighborhood explorer
     @param in a pointer to an input object
     */
    template <class Input, class State, class Move, class CostStructure>
    LateAcceptanceHillClimbing<Input, State, Move, CostStructure>::LateAcceptanceHillClimbing(const Input& in,
                                                                                       StateManager<Input, State, CostStructure>& e_sm,
                                                                                       NeighborhoodExplorer<Input, State, Move, CostStructure>& e_ne,
                                                                                       std::string name)
    : HillClimbing<Input, State, Move, CostStructure>(in, e_sm, e_ne, name)
    {}
    
    template <class Input, class State, class Move, class CostStructure>
    void LateAcceptanceHillClimbing<Input, State, Move, CostStructure>:: RegisterParameters()
    {
      HillClimbing<Input, State, Move, CostStructure>::RegisterParameters();
      steps("steps", "Delay (number of steps in the queue)", this->parameters);
      steps = 10;
    }
    
    /**
     Initializes the run by invoking the companion superclass method, and
     setting the temperature to the start value.
     */
    template <class Input, class State, class Move, class CostStructure>
    void LateAcceptanceHillClimbing<Input, State, Move, CostStructure>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
    {
      HillClimbing<Input, State, Move, CostStructure>::InitializeRun();
      
      // the queue must be filled with the initial state cost at the beginning
      previous_steps = std::vector<CostStructure>(steps, this->current_state_cost);
    }
    
    
    /** A move is surely accepted if it improves the cost function
     or with exponentially decreasing probability if it is
     a worsening one.
     */
    template <class Input, class State, class Move, class CostStructure>
    void LateAcceptanceHillClimbing<Input, State, Move, CostStructure>::SelectMove()
    {
      // TODO: it should become a parameter, the number of neighbors drawn at each iteration (possibly evaluated in parallel)
      const size_t samples = 10;
      size_t sampled;
      CostStructure prev_step_delta_cost = previous_steps[this->iteration % steps] - this->current_state_cost;
      // TODO: check shifting penalty meaningfullness
      EvaluatedMove<Move, CostStructure> em = this->ne.RandomFirst(*this->p_current_state, samples, sampled, [prev_step_delta_cost](const Move& mv, const CostStructure& move_cost) {
        return move_cost <= 0 || move_cost <= prev_step_delta_cost;
      }, this->weights);
      this->current_move = em;
      this->evaluations += sampled;
    }
    
    /**
     A move is randomly picked.
     */
    template <class Input, class State, class Move, class CostStructure>
    void LateAcceptanceHillClimbing<Input, State, Move, CostStructure>::CompleteMove()
    {
      previous_steps[this->iteration % steps] = this->best_state_cost.total;
    }
  }
}

#endif // _LATE_ACCEPTANCE_HILL_CLIMBING_HH_

