#if !defined(_MOVE_RUNNER_HH_)
#define _MOVE_RUNNER_HH_

#include "easylocal/runners/runner.hh"
#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"
#include <boost/signals2.hpp>

namespace EasyLocal {
  
  namespace Core {
    
    /** A Move Runner is an instance of the Runner interface which it compels to
     with a particular definition of @Move (given as template instantiation).
     It is at the root of the inheritance hierarchy of actual runners.
     @ingroup Runners
     */
    template <class Input, class State, class Move, typename CFtype = int, class Compare = std::less<CostStructure<CFtype>>>
    class MoveRunner : public Runner<Input, State, CFtype, Compare>
    {
    public:
      
      typedef Move MoveType;
      
      enum Event { START = 1 << 0, NEW_BEST = 1 << 1, MADE_MOVE = 1 << 2, END = 1 << 3 };
      const size_t events = 4;
      
    public:
      template <typename Observer>
      void registerObserver(Observer&& observer)
      {
        for (unsigned char i = 0; i < events; i++)
          if (observer.events() & (1 << i))
            observers[i].connect(observer);
      }
      
    protected:      
      void notify(Event event) const
      {
        for (unsigned char i = 0; i < events; i++)
          if (event & (1 << i))
            observers[i](event, this->current_state_cost, this->current_move, this->StatusString());
      }
      
      std::vector<boost::signals2::signal<void(Event event, CostStructure<CFtype> current_state_cost, const EvaluatedMove<Move, CFtype>& em, const std::string& status_string)>> observers;
      
    public:
      
      /** Modality of this runner. */
      virtual size_t Modality() const { return ne.Modality(); }
      
      virtual std::string StatusString() const { return std::string("[no status info]"); }
    protected:
      
      /** Constructor.
       @param e_sm */
      MoveRunner(const Input& in, StateManager<Input, State, CFtype>& e_sm,
                 NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
                 std::string name, std::string description);
      
      
      virtual void TerminateRun();
      
      virtual void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
      
      virtual bool AcceptableMoveFound();
      
      
      /** Actions to be perfomed at the beginning of the run. */
      
      /** Encodes the criterion used to select the move at each step. */
      virtual void MakeMove();
      
      void UpdateBestState();
      void UpdateStateCost();
      
      NeighborhoodExplorer<Input, State, Move, CFtype>& ne; /**< A reference to the
                                                             attached neighborhood
                                                             explorer. */
      
      // data
      EvaluatedMove<Move, CFtype> current_move;      /**< The currently selected move. */
      
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    
    template <class Input, class State, class Move, typename CFtype, class Compare>
    void MoveRunner<Input, State, Move, CFtype, Compare>::UpdateBestState()
    {
      if (LessThan(this->current_state_cost.violations, this->best_state_cost.violations)
          || (EqualTo(this->current_state_cost.violations, this->best_state_cost.violations) &&
              (LessThan(this->current_state_cost.total, this->best_state_cost.total))))
      {
        *(this->p_best_state) = *(this->p_current_state);
        this->best_state_cost = this->current_state_cost;
        
        notify(NEW_BEST);
        
        // so that idle iterations are printed correctly
        this->iteration_of_best = this->iteration;
      }
    }
    
    
    template <class Input, class State, class Move, typename CFtype, class Compare>
    MoveRunner<Input, State, Move, CFtype, Compare>::MoveRunner(const Input& in,
                                                       StateManager<Input, State, CFtype>& e_sm,
                                                       NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
                                                       std::string name,
                                                       std::string description)
    : Runner<Input, State, CFtype>(in, e_sm, name, description), observers(events), ne(e_ne)
    {}        
    
    template <class Input, class State, class Move, typename CFtype, class Compare>
    void MoveRunner<Input, State, Move, CFtype, Compare>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
    {
      notify(START);
    }
    
    template <class Input, class State, class Move, typename CFtype, class Compare>
    void MoveRunner<Input, State, Move, CFtype, Compare>::TerminateRun()
    {
      notify(END);
    }
    
    template <class Input, class State, class Move, typename CFtype, class Compare>
    bool MoveRunner<Input, State, Move, CFtype, Compare>::AcceptableMoveFound()
    {
      this->no_acceptable_move_found = !this->current_move.is_valid;
      return this->current_move.is_valid;
    }
    
    /**
     Actually performs the move selected by the local search strategy.
     */
    template <class Input, class State, class Move, typename CFtype, class Compare>
    void MoveRunner<Input, State, Move, CFtype, Compare>::MakeMove()
    {
      if (current_move.is_valid)
      {
        ne.MakeMove(*this->p_current_state, current_move.move);
        this->current_state_cost += current_move.cost;
        notify(MADE_MOVE);
      }
    }
  }
}

#endif /*MOVERUNNER_HH_*/
