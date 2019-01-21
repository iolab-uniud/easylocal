#pragma once

#include "runners/moverunner.hh"

namespace EasyLocal
{
  namespace Core
  {
    /** The Hill Climbing runner considers random move selection. A move
     is then performed only if it does improve or it leaves unchanged
     the value of the cost function.
     @ingroup Runners
     */
    template <class StateManager, class NeighborhoodExplorer>
    class HillClimbing : public MoveRunner<StateManager, NeighborhoodExplorer>
    {
    public:
      UNPACK_MOVERUNNER_BASIC_TYPES()
      
      using MoveRunner<StateManager, NeighborhoodExplorer>::MoveRunner;
      
      ENABLE_RUNNER_CLONE()
      
    protected:
      void InitializeParameters() override
      {
        MoveRunner<StateManager, NeighborhoodExplorer>::InitializeParameters();
        max_idle_iterations("max_idle_iterations", "Total number of allowed idle iterations", this->parameters);
      }
      
      bool MaxIdleIterationExpired() const
      {
        return this->iteration - this->iteration_of_best >= this->max_idle_iterations;
      }
      
      /**
       The select move strategy for the hill climbing simply looks for a
       random move that improves or leaves the cost unchanged.
       */
      void SelectMove(const Input& in) override
      {
        // TODO: it should become a parameter, the number of neighbors drawn at each iteration (possibly evaluated in parallel)
        const size_t samples = 10;
        size_t sampled;
        EvaluatedMove em = this->ne.RandomFirst(in, *this->p_current_state, samples, sampled, [](const Move &mv, const CostStructure &move_cost) {
          return move_cost <= 0;
        },
                                                this->weights);
        this->current_move = em;
        this->evaluations += static_cast<unsigned long int>(sampled);
      }
      
      /**
       The stop criterion is based on the number of iterations elapsed from
       the last strict improvement of the best state cost.
       */
      bool StopCriterion() const override
      {
         return MaxIdleIterationExpired() || this->MaxEvaluationsExpired();
      }
      
      Parameter<unsigned long int> max_idle_iterations;
    };
  } // namespace Core
} // namespace EasyLocal
