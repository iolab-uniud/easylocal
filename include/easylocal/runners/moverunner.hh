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
    template <class Input, class State, class Move, typename CFtype = int>
    class MoveRunner : public Runner<Input, State, CFtype>
    {
    public:
      
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
            observers[i](event, this->current_state_cost, this->current_state_violations, this->current_move);
      }
      
      std::vector<boost::signals2::signal<void(Event event, CFtype cost, CFtype violations, const Move& mv)>> observers;
      
    public:
      
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
      
      void UpdateBestState();
      void UpdateStateCost();
      
      NeighborhoodExplorer<Input, State, Move, CFtype>& ne; /**< A reference to the
                                                             attached neighborhood
                                                             explorer. */
      
      // state data
      Move current_move;      /**< The currently selected move. */
      CFtype current_move_cost; /**< The cost of the selected move. */
      CFtype current_move_violations; /**< The violations of the selected move. */
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
        
        notify(NEW_BEST);
        
        // so that idle iterations are printed correctly
        this->iteration_of_best = this->iteration;
      }
    }
    
    
    template <class Input, class State, class Move, typename CFtype>
    MoveRunner<Input, State, Move, CFtype>::MoveRunner(const Input& in,
                                                       StateManager<Input, State, CFtype>& e_sm,
                                                       NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
                                                       std::string name,
                                                       std::string description)
    : Runner<Input, State, CFtype>(in, e_sm, name, description), observers(events), ne(e_ne)
    {}
    
    template <class Input, class State, class Move, typename CFtype>
    void MoveRunner<Input, State, Move, CFtype>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
    {
      notify(START);
    }
    
    template <class Input, class State, class Move, typename CFtype>
    void MoveRunner<Input, State, Move, CFtype>::TerminateRun()
    {
      notify(END);
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
      notify(MADE_MOVE);
    }
  }
}

#endif /*MOVERUNNER_HH_*/
