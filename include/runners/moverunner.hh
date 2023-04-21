#pragma once

#include "runners/runner.hh"
#include "helpers/solutionmanager.hh"
#include "helpers/neighborhoodexplorer.hh"

namespace EasyLocal
{

namespace Core
{

/** A Move Runner is an instance of the Runner interface which it compels to
     with a particular definition of @Move (given as template instantiation).
     It is at the root of the inheritance hierarchy of actual runners.
     @ingroup Runners
     */
template <class Input, class Solution, class Move, class CostStructure = DefaultCostStructure<int>>
class MoveRunner : public Runner<Input, Solution, CostStructure>
{
public:
  /** Modality of this runner. */
  virtual size_t Modality() const { return ne.Modality(); }

  /** Constructor.
       @param e_sm */
  MoveRunner(const Input &in, SolutionManager<Input, Solution, CostStructure> &e_sm,
             NeighborhoodExplorer<Input, Solution, Move, CostStructure> &e_ne,
             std::string name) : Runner<Input, Solution, CostStructure>(in, e_sm, name), ne(e_ne)
    {}

protected:
  virtual void TerminateRun() override;

  virtual void InitializeRun() override;

  virtual bool AcceptableMoveFound() override;

  /** Actions to be perfomed at the beginning of the run. */

  /** Encodes the criterion used to select the move at each step. */
  virtual void MakeMove() override;

  void UpdateBestState() override final;
  void UpdateStateCost();

  NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne; /**< A reference to the
                                                             attached neighborhood
                                                             explorer. */

  // data
  EvaluatedMove<Move, CostStructure> current_move; /**< The currently selected move. */
};

/*************************************************************************
     * Implementation
     *************************************************************************/

template <class Input, class Solution, class Move, class CostStructure>
void MoveRunner<Input, Solution, Move, CostStructure>::UpdateBestState()
{
  if (LessThan(this->current_state_cost.violations, this->best_state_cost.violations) || (EqualTo(this->current_state_cost.violations, this->best_state_cost.violations) &&
                                                                                          (LessThan(this->current_state_cost.total, this->best_state_cost.total))))
  {
    std::lock_guard<std::mutex> lock(this->best_state_mutex);
    *(this->p_best_state) = *(this->p_current_state);
    this->best_state_cost = this->current_state_cost;

    // so that idle iterations are printed correctly
    this->iteration_of_best = this->iteration;
    // FIXME: write out cost
#if VERBOSE >= 3 
    std::cerr << this->best_state_cost << std::endl;
#endif
  }
}

template <class Input, class Solution, class Move, class CostStructure>
void MoveRunner<Input, Solution, Move, CostStructure>::InitializeRun()
{}

template <class Input, class Solution, class Move, class CostStructure>
void MoveRunner<Input, Solution, Move, CostStructure>::TerminateRun()
{}

template <class Input, class Solution, class Move, class CostStructure>
bool MoveRunner<Input, Solution, Move, CostStructure>::AcceptableMoveFound()
{
  this->no_acceptable_move_found = !this->current_move.is_valid;
  return this->current_move.is_valid;
}

/**
     Actually performs the move selected by the local search strategy.
     */
template <class Input, class Solution, class Move, class CostStructure>
void MoveRunner<Input, Solution, Move, CostStructure>::MakeMove()
{
  if (current_move.is_valid)
  {
    ne.MakeMove(*this->p_current_state, current_move.move);
    this->current_state_cost += current_move.cost;
    //this->logtrace("Runner {}, iteration {}, move {}, move cost {}, current cost {}", this->name, this->iteration, current_move.cost, this->current_state_cost);
  }
}
} // namespace Core
} // namespace EasyLocal
