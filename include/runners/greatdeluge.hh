#pragma once

#include <stdexcept>

#include "runners/moverunner.hh"
#include "helpers/statemanager.hh"
#include "helpers/neighborhoodexplorer.hh"

namespace EasyLocal
{
  namespace Core
  {    
    /** The Great Deluge runner relies on a probabilistic local
     search technique whose name comes from ... the Bible?
     
     The solver is initialized with a minimum water level, at each
     step a candidate move is generated at random, the move is
     accepted if its quality is greater than the water level. After
     the number of neighbors have been sampled at a certain water
     level, the water level is updated.
     The algorithm stops if we have reached the maximum water level
     or if we have done a certain number of non-improving solutions.
     
     In the implementation, the concept of water levels is reversed.
     
     @ingroup Runners
     */
    template <class StateManager, class NeighborhoodExplorer>
    class GreatDeluge : public MoveRunner<StateManager, NeighborhoodExplorer>
    {
    public:
      UNPACK_MOVERUNNER_BASIC_TYPES()
      
      using MoveRunner<StateManager, NeighborhoodExplorer>::MoveRunner;

      ENABLE_RUNNER_CLONE()
      
    protected:
      void InitializeParameters() override
      {
        MoveRunner<StateManager, NeighborhoodExplorer>::InitializeParameters();
        initial_level("initial_level", "Initial water level", this->parameters);
        min_level("min_level", "Minimum water level", this->parameters);
        level_rate("level_rate", "Water decrease factor", this->parameters);
        neighbors_sampled("neighbors_sampled", "Number of neighbors sampled at each water level", this->parameters);
      }
      
      void InitializeRun(const Input& in) override
      {
        MoveRunner<StateManager, NeighborhoodExplorer>::InitializeRun(in);
        level = initial_level * this->current_state_cost.total;
      }
      
      bool StopCriterion() const override
      {
        return level < min_level * this->best_state_cost.total;
      }
      
      void UpdateIterationCounter()
      {
        MoveRunner<StateManager, NeighborhoodExplorer>::UpdateIterationCounter();
        if (this->number_of_iterations % neighbors_sampled == 0)
          level *= level_rate;
      }
      
      void SelectMove(const Input& in) override
      {
        // TODO: it should become a parameter, the number of neighbors drawn at each iteration (possibly evaluated in parallel)
        const size_t samples = 10;
        size_t sampled;
        typename CostStructure::CFtype cur_cost = this->current_state_cost.total;
        double l = level;
        EvaluatedMove em = this->ne.RandomFirst(in, *this->p_current_state, samples, sampled, [cur_cost, l](const Move &mv, const CostStructure &move_cost) {
          return move_cost < 0.0 || move_cost <= l - cur_cost;
        },
                                                this->weights);
        this->current_move = em;
      }
      
      // parameters
      Parameter<double> initial_level;           /**< The initial level. */
      Parameter<double> min_level;               /**< The minimum level. */
      Parameter<double> level_rate;              /**< The level decreasing rate. */
      Parameter<unsigned int> neighbors_sampled; /**< The number of neighbors sampled. */
      // state
      double level; /**< The current level. */
    };
  } // namespace Core
} // namespace EasyLocal
