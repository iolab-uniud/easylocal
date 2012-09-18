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
class MoveRunner
: public Runner<Input,State,CFtype>
{
  friend class RunnerObserver<Input,State,Move,CFtype>;
public:
  // Runner interface
  virtual void Check() const;
  void ResetTimeout();
  void AttachObserver(RunnerObserver<Input,State,Move,CFtype>& ob) { observer = &ob; }
  void InitializeRun(bool first_round = true);
  void TerminateRun();
  Move CurrentMove() const { return current_move; }
  CFtype CurrentMoveCost() const { return current_move_cost; }
  unsigned int Modality() const { return ne.Modality(); }

protected:
  MoveRunner(const Input& in, StateManager<Input,State,CFtype>& e_sm,
             NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
             std::string name);

  /* state manipulations */
  virtual void GoCheck() const = 0;
  /** Actions to be perfomed at the beginning of the run. */
  virtual void ComputeMoveCost();
  
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
                                                std::string name)
  : Runner<Input,State,CFtype>(in, e_sm, name), ne(e_ne), observer(NULL)
{}

template <class Input, class State, class Move, typename CFtype>
void MoveRunner<Input,State,Move,CFtype>::InitializeRun(bool first_round) 
{
  Runner<Input,State,CFtype>::InitializeRun();
  if (observer != NULL)
    observer->NotifyStartRunner(*this);
}


template <class Input, class State, class Move, typename CFtype>
void MoveRunner<Input,State,Move,CFtype>::TerminateRun() 
{
  Runner<Input,State,CFtype>::TerminateRun();
  if (observer != NULL)
		observer->NotifyEndRunner(*this);
}


/**
   Checks wether the object state is consistent with all the related
   objects.
*/
template <class Input, class State, class Move, typename CFtype>
void MoveRunner<Input,State,Move,CFtype>::Check() const
{}

/**
   Actually performs the move selected by the local search strategy.
 */
template <class Input, class State, class Move, typename CFtype>
void MoveRunner<Input,State,Move,CFtype>::MakeMove()
{ 
	ne.MakeMove(this->current_state, current_move);  
}


/**
   Computes the cost of the selected move; it delegates this task to the
   neighborhood explorer.
*/
template <class Input, class State, class Move, typename CFtype>
void MoveRunner<Input,State,Move,CFtype>::ComputeMoveCost()
{ 
	current_move_cost = ne.DeltaCostFunction(this->current_state, current_move); 
}

/**
   Updates the cost of the internal state of the runner.
*/
template <class Input, class State, class Move, typename CFtype>
void MoveRunner<Input,State,Move,CFtype>::UpdateStateCost()
{ 
  // std:: cerr << this->current_state_cost << std::endl; 
	this->current_state_cost += current_move_cost; 
//  	std:: cerr << current_move_cost << " " << this->current_state_cost << std::endl; 
}

#endif // _MOVE_RUNNER_HH_
