#if !defined(_MOVE_RUNNER_HH_)
#define _MOVE_RUNNER_HH_

#include <runners/Runner.hh>
#include <helpers/StateManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <observers/RunnerObserver.hh>
#include <testers/Tester.hh>

/** A Move Runner is an instance of the Runner interface which it compels to
 with a particular definition of @Move (given as template instantiation).
 It is at the root of the inheritance hierarchy of actual runners.
 @ingroup Runners
 */

template <class Input, class State, class Move, typename CFtype = int>
class MoveRunner : public Runner<Input,State,CFtype>
{
  friend class RunnerObserver<Input,State,Move,CFtype>;
public:
  // Runner interface
  void ResetTimeout();
  void AttachObserver(RunnerObserver<Input,State,Move,CFtype>& ob) { observer = &ob; }
  Move CurrentMove() const { return current_move; }
  CFtype CurrentMoveCost() const { return current_move_cost; }
  unsigned int Modality() const { return ne.Modality(); }    
protected:
  MoveRunner(const Input& in, StateManager<Input,State,CFtype>& e_sm,
             NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
             std::string name, std::string description);
  
  
  virtual void TerminateRun();
  
  virtual void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
  

  /** Actions to be perfomed at the beginning of the run. */
  
  /** Encodes the criterion used to select the move at each step. */
  virtual void MakeMove();

  void UpdateStateCost();
  
  NeighborhoodExplorer<Input,State,Move,CFtype>& ne; /**< A reference to the
                                                      attached neighborhood 
                                                      explorer. */
  
  // state data
  Move current_move;      /**< The currently selected move. */
  CFtype current_move_cost; /**< The cost of the selected move. */
  
  RunnerObserver<Input,State,Move,CFtype>* observer;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class State, class Move, typename CFtype>
MoveRunner<Input,State,Move,CFtype>::MoveRunner(const Input& in, 
                                                StateManager<Input,State,CFtype>& e_sm,
                                                NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                std::string name,
                                                std::string description)
: Runner<Input,State,CFtype>(in, e_sm, name, description), ne(e_ne), observer(nullptr)
{}

template <class Input, class State, class Move, typename CFtype>
void MoveRunner<Input,State,Move,CFtype>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
{
  if (observer != nullptr)
    observer->NotifyStartRunner(*this);
}


template <class Input, class State, class Move, typename CFtype>
void MoveRunner<Input,State,Move,CFtype>::TerminateRun() 
{
  if (observer != nullptr)
		observer->NotifyEndRunner(*this);
}

/**
 Actually performs the move selected by the local search strategy.
 */
template <class Input, class State, class Move, typename CFtype>
void MoveRunner<Input,State,Move,CFtype>::MakeMove()
{ 
	ne.MakeMove(*this->p_current_state, current_move);
  this->current_state_cost += current_move_cost;
  if (observer != nullptr)
		observer->NotifyMadeMove(*this);
}


#endif /*MOVERUNNER_HH_*/
