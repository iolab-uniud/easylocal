#pragma once

#include "easylocal/runners/moverunner.hh"
#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"
#include "easylocal/utils/types.hh"

namespace EasyLocal
{

namespace Core
{

/** The Steepest Descent runner performs a simple local search.
     At each step of the search, the best move in the neighborhood of current
     solution is selected and performed.
     It is worth noticing that this algorithm leads straightly to the
     nearest local minimum of a given state.
     @ingroup Runners
     */
template <class Input, class State, class Move, class CostStructure = DefaultCostStructure<int>>
class SteepestDescent : public MoveRunner<Input, State, Move, CostStructure>
{
public:
  using MoveRunner<Input, State, Move, CostStructure>::MoveRunner;

protected:
  void StoreMove();
  bool StopCriterion();
  void SelectMove();
};

/*************************************************************************
     * Implementation
     *************************************************************************/

/**
     Selects always the best move in the neighborhood.
     */
template <class Input, class State, class Move, class CostStructure>
void SteepestDescent<Input, State, Move, CostStructure>::SelectMove()
{
  size_t explored;
  EvaluatedMove<Move, CostStructure> em = this->ne.SelectBest(*this->p_current_state, explored, [](const Move &mv, const CostStructure &move_cost) {
    return move_cost < 0;
  },
                                                              this->weights);
  this->current_move = em;
  this->evaluations += static_cast<unsigned long int>(explored);
}

/**
     The search is stopped when no (strictly) improving move has been found.
     */
template <class Input, class State, class Move, class CostStructure>
bool SteepestDescent<Input, State, Move, CostStructure>::StopCriterion()
{
  return this->iteration > 0 && !this->current_move.is_valid;
}
} // namespace Core
} // namespace EasyLocal
