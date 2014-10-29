#if !defined(_MOVE_RUNNER_HH_)
#define _MOVE_RUNNER_HH_

#include "easylocal/runners/runner.hh"
#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"

#include "easylocal/observers/runnerobserver.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** A Move Runner is an instance of the Runner interface which it compels to
    with a particular definition of @Move (given as template instantiation).
    It is at the root of the inheritance hierarchy of actual runners.
    @ingroup Runners
    */
    template <class Input, class State, class Move, typename CFtype>
    class MoveRunner : public Runner<Input, State, CFtype>
    {
      friend class Debug::RunnerObserver<Input, State, Move, CFtype>;
  
    public:
  
      /** Attaches an observer to this runner.
      @param ob a RunnerObserver of a compliant type
      */
      void AttachObserver(Debug::RunnerObserver<Input, State, Move, CFtype>& ob)
      {
        observer = &ob;
      }
  
      /** Modality of this runner. */
      virtual unsigned int Modality() const { return ne.Modality(); }
  
      virtual std::string StatusString() { return std::string("[no status info]"); }
    protected:
  
      /** Constructor.
      @param e_sm */
      MoveRunner(const Input& in, StateManager<Input, State, CFtype>& e_sm,
      NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
      std::string name, std::string description);
  
  
      virtual void TerminateRun();
  
      virtual void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
  
  
      /** Actions to be perfomed at the beginning of the run. */
  
      /** Encodes the criterion used to select the move at each step. */
      virtual void MakeMove();
      
      virtual void SelectRandomMove();
  
      void UpdateBestState();
      void UpdateStateCost();
  
      NeighborhoodExplorer<Input, State, Move, CFtype>& ne; /**< A reference to the
        attached neighborhood
        explorer. */
  
        // state data
        Move current_move;      /**< The currently selected move. */
      CFtype current_move_cost; /**< The cost of the selected move. */
      CFtype current_move_violations; /**< The violations of the selected move. */
  
      Debug::RunnerObserver<Input, State, Move, CFtype>* observer;
  
  
    };

    /*************************************************************************
    * Implementation
    *************************************************************************/


    template <class Input, class State, class Move, typename CFtype>
    void MoveRunner<Input, State, Move, CFtype>::UpdateBestState()
    {
      if (LessThan(this->current_state_violations, this->best_state_violations)
	  || (EqualTo(this->current_state_violations, this->best_state_violations) &&
	      (LessThan(this->current_state_cost, this->best_state_cost))))
      {
        *(this->p_best_state) = *(this->p_current_state);
        this->best_state_cost = this->current_state_cost;
        this->best_state_violations = this->current_state_violations;

        if (this->observer != nullptr)
          this->observer->NotifyNewBest(*this);
 
        // so that idle iterations are printed correctly
        this->iteration_of_best = this->iteration;
      }
      //   }
    }


    template <class Input, class State, class Move, typename CFtype>
    MoveRunner<Input, State, Move, CFtype>::MoveRunner(const Input& in,
    StateManager<Input, State, CFtype>& e_sm,
    NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
    std::string name,
    std::string description)
      : Runner<Input, State, CFtype>(in, e_sm, name, description), ne(e_ne), observer(nullptr)
        { }

    template <class Input, class State, class Move, typename CFtype>
    void MoveRunner<Input, State, Move, CFtype>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
    {
      if (observer != nullptr)
        observer->NotifyStartRunner(*this);
    }

    template <class Input, class State, class Move, typename CFtype>
    void MoveRunner<Input, State, Move, CFtype>::TerminateRun()
    {
      if (observer != nullptr)
        observer->NotifyEndRunner(*this);
    }

    /**
    Actually performs the move selected by the local search strategy.
    */
    template <class Input, class State, class Move, typename CFtype>
    void MoveRunner<Input, State, Move, CFtype>::MakeMove()
    {
      ne.MakeMove(*this->p_current_state, current_move);
      this->current_state_cost += current_move_cost;
      this->current_state_violations += current_move_violations;
      if (observer != nullptr) {
        observer->NotifyMadeMove(*this);
      }
    }
    
    
    template <class Input, class State, class Move, typename CFtype>
    void MoveRunner<Input, State, Move, CFtype>::SelectRandomMove()
    {
      this->ne.RandomMove(*this->p_current_state, this->current_move);
      this->current_move_violations = this->ne.DeltaViolations(*this->p_current_state, this->current_move);
      this->current_move_cost = this->ne.DeltaObjective(*this->p_current_state, this->current_move) 
	+ HARD_WEIGHT * this->current_move_violations;
    }
  }
}

#endif /*MOVERUNNER_HH_*/
