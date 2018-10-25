#pragma once

#include "easylocal/runners/runner.hh"
#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"

namespace EasyLocal
{
  
  namespace Core
  {
    
    /** A Move Runner is an instance of the Runner interface which it compels to
     with a particular definition of @Move (given as template instantiation).
     It is at the root of the inheritance hierarchy of actual runners.
     @ingroup Runners
     */
    template <class Input, class State, class _Move, class CostStructure = DefaultCostStructure<int>>
    class MoveRunner : public Runner<Input, State, CostStructure>
    {
    public:
      typedef _Move Move;
      /** Modality of this runner. */
      virtual size_t Modality() const { return ne.Modality(); }
      
      /** Constructor.
       @param in the Input object
       @param sm the State Manager
       @param ne the Neighborhood Explorer
       @param name the name of the runner
       @param desc a description of the runner
       @deprecated
       */
      [[deprecated("This is the old style easylocal interface, it might still be used, however we advise to upgrade to Input-less class and Input-aware methods")]]
      MoveRunner(const Input &in, StateManager<Input, State, CostStructure> &sm,
                 NeighborhoodExplorer<Input, State, Move, CostStructure> &ne,
                 std::string name, std::string description);
      
      /** Constructor.
       @param sm the State Manager
       @param ne the Neighborhood Explorer
       @param name the name of the runner
       @param desc a description of the runner
       */
      MoveRunner(StateManager<Input, State, CostStructure> &sm,
                 NeighborhoodExplorer<Input, State, Move, CostStructure> &ne,
                 std::string name, std::string description);
      
    protected:
      virtual bool AcceptableMoveFound(const Input& in);
      
      /** Actions to be perfomed at the beginning of the run. */
      
      /** Encodes the criterion used to select the move at each step. */
      virtual void MakeMove(const Input& in);
      
      void UpdateBestState() final;
      void UpdateStateCost();
      
      NeighborhoodExplorer<Input, State, Move, CostStructure> &ne; /**< A reference to the
                                                                    attached neighborhood
                                                                    explorer. */
      
      // data
      EvaluatedMove<Move, CostStructure> current_move; /**< The currently selected move. */
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class State, class Move, class CostStructure>
    void MoveRunner<Input, State, Move, CostStructure>::UpdateBestState()
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
    
    template <class Input, class State, class Move, class CostStructure>
    MoveRunner<Input, State, Move, CostStructure>::MoveRunner(const Input &in,
                                                              StateManager<Input, State, CostStructure> &sm,
                                                              NeighborhoodExplorer<Input, State, Move, CostStructure> &ne,
                                                              std::string name,
                                                              std::string description)
    : Runner<Input, State, CostStructure>(in, sm, name, description), ne(ne)
    {}
    
    template <class Input, class State, class Move, class CostStructure>
    MoveRunner<Input, State, Move, CostStructure>::MoveRunner(StateManager<Input, State, CostStructure> &sm,
                                                              NeighborhoodExplorer<Input, State, Move, CostStructure> &ne,
                                                              std::string name,
                                                              std::string description)
    : Runner<Input, State, CostStructure>(sm, name, description), ne(ne)
    {}
    
    template <class Input, class State, class Move, class CostStructure>
    bool MoveRunner<Input, State, Move, CostStructure>::AcceptableMoveFound(const Input& in)
    {
      this->no_acceptable_move_found = !this->current_move.is_valid;
      return this->current_move.is_valid;
    }
    
    /**
     Actually performs the move selected by the local search strategy.
     */
    template <class Input, class State, class Move, class CostStructure>
    void MoveRunner<Input, State, Move, CostStructure>::MakeMove(const Input& in)
    {
      if (current_move.is_valid)
      {
        ne.MakeMove(in, *this->p_current_state, current_move.move);
        this->current_state_cost += current_move.cost;
        //this->logtrace("Runner {}, iteration {}, move {}, move cost {}, current cost {}", this->name, this->iteration, current_move.cost, this->current_state_cost);
      }
    }
  } // namespace Core
} // namespace EasyLocal
