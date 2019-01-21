#pragma once

#include "runners/hillclimbing.hh"

namespace EasyLocal
{
  namespace Core
  {
    /** The Late Acceptance Hill Climbing maintains a list of previous
     moves and defers acceptance to k steps further.
     
     @ingroup Runners
     */
    template <class StateManager, class NeighborhoodExplorer>
    class LateAcceptanceHillClimbing : public HillClimbing<StateManager, NeighborhoodExplorer>
    {
    public:
      UNPACK_MOVERUNNER_BASIC_TYPES()
      
      using HillClimbing<StateManager, NeighborhoodExplorer>::HillClimbing;
      
      ENABLE_RUNNER_CLONE()
      
    protected:
      
      /**
       Initializes the run by invoking the companion superclass method, and
       setting the number of previous steps to the start value.
       */
      void InitializeRun(const Input& in) override
      {
        HillClimbing<StateManager, NeighborhoodExplorer>::InitializeRun(in);
        if (steps <= 0)
          throw IncorrectParameterValue(steps, "should be greater than zero");
        
        // the queue must be filled with the initial state cost at the beginning
        previous_steps = std::vector<CostStructure>(steps, this->current_state_cost);
      }
      
      void CompleteMove(const Input& in) override
      {
        previous_steps[this->iteration % steps] = this->best_state_cost;
      }
      
      void SelectMove(const Input& in) override
      {
        // TODO: it should become a parameter, the number of neighbors drawn at each iteration (possibly evaluated in parallel)
        const size_t samples = 10;
        size_t sampled;
        CostStructure prev_step_delta_cost = previous_steps[this->iteration % steps] - this->current_state_cost;
        // TODO: check shifting penalty meaningfullness
        EvaluatedMove em = this->ne.RandomFirst(in, *this->p_current_state, samples, sampled, [prev_step_delta_cost](const Move &mv, const CostStructure &move_cost) {
          return move_cost <= 0 || move_cost <= prev_step_delta_cost;
        },
                                                                     this->weights);
        this->current_move = em;
        this->evaluations += sampled;
      }
      
      // parameters
      void InitializeParameters() override
      {
        HillClimbing<StateManager, NeighborhoodExplorer>::InitializeParameters();
        steps("steps", "Delay (number of steps in the queue)", this->parameters);
        steps = 10;
      }
      
      Parameter<unsigned int> steps;
      std::vector<CostStructure> previous_steps;
    };
  } // namespace Core
} // namespace EasyLocal
