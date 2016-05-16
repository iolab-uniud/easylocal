#pragma once

#include "easylocal/runners/runner.hh"
#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** A Move Runner is an instance of the Runner interface which it compels to
     with a particular definition of @Move (given as template instantiation).
     It is at the root of the inheritance hierarchy of actual runners.
     @ingroup Runners
     */
    template <class Input, class State, class Move, class CostStructure = DefaultCostStructure<int>>
    class MoveRunner : public Runner<Input, State, CostStructure>
    {
    public:
      /** Modality of this runner. */
      virtual size_t Modality() const { return ne.Modality(); }
      
      /** Constructor.
       @param e_sm */
      MoveRunner(const Input& in, StateManager<Input, State, CostStructure>& e_sm,
                 NeighborhoodExplorer<Input, State, Move, CostStructure>& e_ne,
                 std::string name, std::shared_ptr<spdlog::logger> logger);
      
    protected:
            
      virtual void TerminateRun();
      
      virtual void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
      
      virtual bool AcceptableMoveFound();
      
      
      /** Actions to be perfomed at the beginning of the run. */
      
      /** Encodes the criterion used to select the move at each step. */
      virtual void MakeMove();
      
      void UpdateBestState();
      void UpdateStateCost();
      
      NeighborhoodExplorer<Input, State, Move, CostStructure>& ne; /**< A reference to the
                                                             attached neighborhood
                                                             explorer. */
      
      // data
      EvaluatedMove<Move, CostStructure> current_move;      /**< The currently selected move. */
      
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    
    template <class Input, class State, class Move, class CostStructure>
    void MoveRunner<Input, State, Move, CostStructure>::UpdateBestState()
    {
      if (LessThan(this->current_state_cost.violations, this->best_state_cost.violations)
          || (EqualTo(this->current_state_cost.violations, this->best_state_cost.violations) &&
              (LessThan(this->current_state_cost.total, this->best_state_cost.total))))
      {
        *(this->p_best_state) = *(this->p_current_state);
        this->best_state_cost = this->current_state_cost;
        
        this->loginfo("Runner {}, new best cost found {} at iteration {}", this->name, this->best_state_cost, this->iteration);
        
        // so that idle iterations are printed correctly
        this->iteration_of_best = this->iteration;
      }
    }
    
    
    template <class Input, class State, class Move, class CostStructure>
    MoveRunner<Input, State, Move, CostStructure>::MoveRunner(const Input& in,
                                                              StateManager<Input, State, CostStructure>& e_sm,
                                                              NeighborhoodExplorer<Input, State, Move, CostStructure>& e_ne,
                                                              std::string name,
                                                              std::shared_ptr<spdlog::logger> logger)
    : Runner<Input, State, CostStructure>(in, e_sm, name, logger), ne(e_ne)
    {}        
    
    template <class Input, class State, class Move, class CostStructure>
    void MoveRunner<Input, State, Move, CostStructure>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
    {
      this->loginfo("Runner {}, run initialized", this->name);
    }
    
    template <class Input, class State, class Move, class CostStructure>
    void MoveRunner<Input, State, Move, CostStructure>::TerminateRun()
    {
      this->loginfo("Runner {}, run terminated", this->name);
    }
    
    template <class Input, class State, class Move, class CostStructure>
    bool MoveRunner<Input, State, Move, CostStructure>::AcceptableMoveFound()
    {
      this->no_acceptable_move_found = !this->current_move.is_valid;
      return this->current_move.is_valid;
    }
    
    /**
     Actually performs the move selected by the local search strategy.
     */
    template <class Input, class State, class Move, class CostStructure>
    void MoveRunner<Input, State, Move, CostStructure>::MakeMove()
    {
      if (current_move.is_valid)
      {
        ne.MakeMove(*this->p_current_state, current_move.move);
        this->current_state_cost += current_move.cost;
        this->logtrace("Runner {}, iteration {}, move {}, move cost {}, current cost {}", this->name, this->iteration, current_move.cost, this->current_state_cost);
      }
    }
  }
}

