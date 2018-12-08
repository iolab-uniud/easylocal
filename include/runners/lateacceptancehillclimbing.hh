#pragma once

#include <stdexcept>

#include "runners/hillclimbing.hh"
#include "helpers/statemanager.hh"
#include "helpers/neighborhoodexplorer.hh"

namespace EasyLocal
{
  
  namespace Core
  {
    
    /** The Late Acceptance Hill Climbing maintains a list of previous
     moves and defers acceptance to k steps further.
     
     @ingroup Runners
     */
    template <class Input, class State, class Move, class CostStructure = DefaultCostStructure<int>>
    class LateAcceptanceHillClimbing : public HillClimbing<Input, State, Move, CostStructure>
    {
    public:
      using HillClimbing<Input, State, Move, CostStructure>::HillClimbing;
      std::unique_ptr<Runner<Input, State, CostStructure>> Clone() const override;
      
    protected:
      void InitializeRun(const Input& in) override;
      void CompleteMove(const Input& in) override;
      void SelectMove(const Input& in) override;
      
      // parameters
      void InitializeParameters() override;
      Parameter<unsigned int> steps;
      std::vector<CostStructure> previous_steps;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class State, class Move, class CostStructure>
    void LateAcceptanceHillClimbing<Input, State, Move, CostStructure>::InitializeParameters()
    {
      HillClimbing<Input, State, Move, CostStructure>::InitializeParameters();
      steps("steps", "Delay (number of steps in the queue)", this->parameters);
      steps = 10;
    }
    
    /**
     Initializes the run by invoking the companion superclass method, and
     setting the temperature to the start value.
     */
    template <class Input, class State, class Move, class CostStructure>
    void LateAcceptanceHillClimbing<Input, State, Move, CostStructure>::InitializeRun(const Input& in)
    {
      HillClimbing<Input, State, Move, CostStructure>::InitializeRun(in);
      if (steps <= 0)
        throw IncorrectParameterValue(steps, "should be greater than zero");
      
      // the queue must be filled with the initial state cost at the beginning
      previous_steps = std::vector<CostStructure>(steps, this->current_state_cost);
    }
    
    /** A move is surely accepted if it improves the cost function
     or with exponentially decreasing probability if it is
     a worsening one.
     */
    template <class Input, class State, class Move, class CostStructure>
    void LateAcceptanceHillClimbing<Input, State, Move, CostStructure>::SelectMove(const Input& in)
    {
      // TODO: it should become a parameter, the number of neighbors drawn at each iteration (possibly evaluated in parallel)
      const size_t samples = 10;
      size_t sampled;
      CostStructure prev_step_delta_cost = previous_steps[this->iteration % steps] - this->current_state_cost;
      // TODO: check shifting penalty meaningfullness
      EvaluatedMove<Move, CostStructure> em = this->ne.RandomFirst(in, *this->p_current_state, samples, sampled, [prev_step_delta_cost](const Move &mv, const CostStructure &move_cost) {
        return move_cost <= 0 || move_cost <= prev_step_delta_cost;
      },
                                                                   this->weights);
      this->current_move = em;
      this->evaluations += sampled;
    }
    
    /**
     A move is randomly picked.
     */
    template <class Input, class State, class Move, class CostStructure>
    void LateAcceptanceHillClimbing<Input, State, Move, CostStructure>::CompleteMove(const Input& in)
    {
      previous_steps[this->iteration % steps] = this->best_state_cost;
    }
    
    template <class Input, class State, class Move, class CostStructure>
    std::unique_ptr<Runner<Input, State, CostStructure>> LateAcceptanceHillClimbing<Input, State, Move, CostStructure>::Clone() const
    {
      return Runner<Input, State, CostStructure>::MakeClone(this);
    }
  } // namespace Core
} // namespace EasyLocal
