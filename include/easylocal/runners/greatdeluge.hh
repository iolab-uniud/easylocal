#pragma once

#include <stdexcept>

#include "easylocal/runners/moverunner.hh"
#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"

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
    template <class Input, class State, class Move, class CostStructure = DefaultCostStructure<int>>
    class GreatDeluge : public MoveRunner<Input, State, Move, CostStructure>
    {
    public:
      typedef typename CostStructure::CFtype CFtype;
      
      using MoveRunner<Input, State, Move, CostStructure>::MoveRunner;
      std::unique_ptr<Runner<Input, State, CostStructure>> Clone() const override;
      
    protected:
      void InitializeParameters() override;
      void InitializeRun(const Input& in) override;
      bool StopCriterion() const override;
      void UpdateIterationCounter();
      void SelectMove(const Input& in) override;
      
      // parameters
      Parameter<double> initial_level;           /**< The initial level. */
      Parameter<double> min_level;               /**< The minimum level. */
      Parameter<double> level_rate;              /**< The level decreasing rate. */
      Parameter<unsigned int> neighbors_sampled; /**< The number of neighbors sampled. */
      // state
      double level; /**< The current level. */
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class State, class Move, class CostStructure>
    void GreatDeluge<Input, State, Move, CostStructure>::InitializeParameters()
    {
      MoveRunner<Input, State, Move, CostStructure>::InitializeParameters();
      initial_level("initial_level", "Initial water level", this->parameters);
      min_level("min_level", "Minimum water level", this->parameters);
      level_rate("level_rate", "Water decrease factor", this->parameters);
      neighbors_sampled("neighbors_sampled", "Number of neighbors sampled at each water level", this->parameters);
    }
    
    /**
     Initializes the run by invoking the companion superclass method, and
     setting current level to the initial one.
     */
    template <class Input, class State, class Move, class CostStructure>
    void GreatDeluge<Input, State, Move, CostStructure>::InitializeRun(const Input& in)
    {
      MoveRunner<Input, State, Move, CostStructure>::InitializeRun(in);
      level = initial_level * this->current_state_cost.total;
    }
    
    /**
     A move is randomly picked and its cost is stored.
     */
    template <class Input, class State, class Move, class CostStructure>
    void GreatDeluge<Input, State, Move, CostStructure>::SelectMove(const Input& in)
    {
      // TODO: it should become a parameter, the number of neighbors drawn at each iteration (possibly evaluated in parallel)
      const size_t samples = 10;
      size_t sampled;
      CFtype cur_cost = this->current_state_cost.total;
      double l = level;
      EvaluatedMove<Move, CostStructure> em = this->ne.RandomFirst(in, *this->p_current_state, samples, sampled, [cur_cost, l](const Move &mv, const CostStructure &move_cost) {
        return move_cost < 0.0 || move_cost <= l - cur_cost;
      },
                                                                   this->weights);
      this->current_move = em;
    }
    
    /**
     The search stops when a low temperature has reached.
     */
    template <class Input, class State, class Move, class CostStructure>
    bool GreatDeluge<Input, State, Move, CostStructure>::StopCriterion() const
    {
      return level < min_level * this->best_state_cost.total;
    }
    
    /**
     At regular steps, the temperature is decreased
     multiplying it by a cooling rate.
     */
    template <class Input, class State, class Move, class CostStructure>
    void GreatDeluge<Input, State, Move, CostStructure>::UpdateIterationCounter()
    {
      MoveRunner<Input, State, Move, CostStructure>::UpdateIterationCounter();
      if (this->number_of_iterations % neighbors_sampled == 0)
        level *= level_rate;
    }
    
    template <class Input, class State, class Move, class CostStructure>
    std::unique_ptr<Runner<Input, State, CostStructure>> GreatDeluge<Input, State, Move, CostStructure>::Clone() const
    {
      return std::make_unique<GreatDeluge<Input, State, Move, CostStructure>>(*this);
    }
  } // namespace Core
} // namespace EasyLocal
