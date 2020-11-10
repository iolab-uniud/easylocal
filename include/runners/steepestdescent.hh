#pragma once

#include "runners/moverunner.hh"
#include "helpers/solutionmanager.hh"
#include "helpers/neighborhoodexplorer.hh"
#include "utils/types.hh"

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
template <class Input, class Solution, class Move, class CostStructure = DefaultCostStructure<int>>
class SteepestDescent : public MoveRunner<Input, Solution, Move, CostStructure>
{
public:
  using MoveRunner<Input, Solution, Move, CostStructure>::MoveRunner;

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
template <class Input, class Solution, class Move, class CostStructure>
void SteepestDescent<Input, Solution, Move, CostStructure>::SelectMove()
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
template <class Input, class Solution, class Move, class CostStructure>
bool SteepestDescent<Input, Solution, Move, CostStructure>::StopCriterion()
{
  return this->iteration > 0 && !this->current_move.is_valid;
}
} // namespace Core
} // namespace EasyLocal
