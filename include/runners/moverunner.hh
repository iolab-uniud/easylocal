#pragma once

#include "runners/runner.hh"
#include "helpers/statemanager.hh"
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
    template <class StateManager, class NeighborhoodExplorer>
    class MoveRunner : public Runner<typename NeighborhoodExplorer::Input, typename NeighborhoodExplorer::State, typename NeighborhoodExplorer::CostStructure>
    {
    public:
#if !defined(UNPACK_MOVERUNNER_BASIC_TYPES)
#define UNPACK_MOVERUNNER_BASIC_TYPES() \
      using Input = typename NeighborhoodExplorer::Input; \
      using State = typename NeighborhoodExplorer::State; \
      using Move = typename NeighborhoodExplorer::Move; \
      using CostStructure = typename NeighborhoodExplorer::CostStructure; \
      using EvaluatedMove = typename NeighborhoodExplorer::EvaluatedMove;
#endif
      
      
      UNPACK_MOVERUNNER_BASIC_TYPES()
      
      /** Modality of this runner. */
      size_t Modality() const { return ne.Modality(); }
      
      /** Constructor.
       @param sm the State Manager
       @param ne the Neighborhood Explorer
       @param name the name of the runner
       @param desc a description of the runner
       */
      MoveRunner(StateManager& sm,
                 NeighborhoodExplorer& ne,
                 std::string name, std::string description)
      : Runner<Input, State, CostStructure>(sm, name, description), ne(ne)
      {}
      
    protected:
      virtual bool AcceptableMoveFound(const Input& in)
      {
        this->no_acceptable_move_found = !this->current_move.is_valid;
        return this->current_move.is_valid;
      }
      
      /** Actions to be perfomed at the beginning of the run. */
      
      /** Encodes the criterion used to select the move at each step. */
      virtual void MakeMove(const Input& in)
      {
        if (current_move.is_valid)
        {
          ne.MakeMove(in, *this->p_current_state, current_move.move);
          this->current_state_cost += current_move.cost;
          //this->logtrace("Runner {}, iteration {}, move {}, move cost {}, current cost {}", this->name, this->iteration, current_move.cost, this->current_state_cost);
        }
      }
      
      void UpdateBestState() final
      {
        if (LessThan(this->current_state_cost.violations, this->best_state_cost.violations) || (EqualTo(this->current_state_cost.violations, this->best_state_cost.violations) &&
                                                                                                (LessThan(this->current_state_cost.total, this->best_state_cost.total))))
        {
          std::lock_guard<std::mutex> lock(this->best_state_mutex);
          *(this->p_best_state) = *(this->p_current_state);
          this->best_state_cost = this->current_state_cost;
          
          // so that idle iterations are printed correctly
          this->iteration_of_best = this->iteration;
        }

      }
      
      NeighborhoodExplorer& ne; /**< A reference to the
                                 attached neighborhood
                                 explorer. */
      
      // data
      EvaluatedMove current_move; /**< The currently selected move. */
    };
  } // namespace Core
} // namespace EasyLocal
