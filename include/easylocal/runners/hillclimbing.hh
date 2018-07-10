#pragma once

#include <stdexcept>

#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"
#include "easylocal/runners/moverunner.hh"

namespace EasyLocal
{

namespace Core
{

/** The Hill Climbing runner considers random move selection. A move
     is then performed only if it does improve or it leaves unchanged
     the value of the cost function.
     @ingroup Runners
     */
template <class Input, class State, class Move, class CostStructure = DefaultCostStructure<int>>
class HillClimbing : public MoveRunner<Input, State, Move, CostStructure>
{
public:
  using MoveRunner<Input, State, Move, CostStructure>::MoveRunner;

protected:
  Parameter<unsigned long int> max_idle_iterations;
  void InitializeParameters();
  bool MaxIdleIterationExpired() const;
  bool StopCriterion();
  void SelectMove();
  // parameters
};

/*************************************************************************
     * Implementation
     *************************************************************************/

template <class Input, class State, class Move, class CostStructure>
void HillClimbing<Input, State, Move, CostStructure>::InitializeParameters()
{
  MoveRunner<Input, State, Move, CostStructure>::InitializeParameters();
  max_idle_iterations("max_idle_iterations", "Total number of allowed idle iterations", this->parameters);
}

/**
     The select move strategy for the hill climbing simply looks for a
     random move that improves or leaves the cost unchanged.
     */
template <class Input, class State, class Move, class CostStructure>
void HillClimbing<Input, State, Move, CostStructure>::SelectMove()
{
  // TODO: it should become a parameter, the number of neighbors drawn at each iteration (possibly evaluated in parallel)
  const size_t samples = 10;
  size_t sampled;
  EvaluatedMove<Move, CostStructure> em = this->ne.RandomFirst(*this->p_current_state, samples, sampled, [](const Move &mv, const CostStructure &move_cost) {
    return move_cost <= 0;
  },
                                                               this->weights);
  this->current_move = em;
  this->evaluations += static_cast<unsigned long int>(sampled);
}

template <class Input, class State, class Move, class CostStructure>
bool HillClimbing<Input, State, Move, CostStructure>::MaxIdleIterationExpired() const
{
  return this->iteration - this->iteration_of_best >= this->max_idle_iterations;
}

/**
     The stop criterion is based on the number of iterations elapsed from
     the last strict improvement of the best state cost.
     */
template <class Input, class State, class Move, class CostStructure>
bool HillClimbing<Input, State, Move, CostStructure>::StopCriterion()
{
  return MaxIdleIterationExpired() || this->MaxEvaluationsExpired();
}

} // namespace Core
} // namespace EasyLocal
