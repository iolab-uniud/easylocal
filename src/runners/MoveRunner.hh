#ifndef MOVERUNNER_HH_
#define MOVERUNNER_HH_

#include "Runner.hh"
#include "../helpers/StateManager.hh"
#include "../helpers/NeighborhoodExplorer.hh"
#include "../basics/EasyLocalException.hh"

/** A Move Runner is an instance of the Runner interface which it compels to
    with a particular definition of @Move (given as template instantiation).
    It is at the root of the inheritance hierarchy of actual runners.
    @ingroup Runners
*/

template <class Input, class State, class Move, typename CFtype = int>
class MoveRunner
            : public Runner<Input,State,CFtype>
{
public:
    // Runner interface
    virtual void Check() const throw(EasyLocalException);
    void ResetTimeout();
protected:
   MoveRunner(const Input& in, StateManager<Input,State,CFtype>& e_sm,
               NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
               const std::string& name = "");
    /* state manipulations */
    virtual void GoCheck() const throw(EasyLocalException) = 0;
    /** Actions to be perfomed at the beginning of the run. */
    virtual void ComputeMoveCost();

    /** Encodes the criterion used to select the move at each step. */
    //    virtual void SelectMove() = 0;
    virtual void MakeMove();
    void UpdateStateCost();
 
  NeighborhoodExplorer<Input,State,Move,CFtype>& ne; /**< A pointer to the
    		      attached neighborhood 
    		      explorer. */

    // state data
    Move current_move;      /**< The currently selected move. */
    CFtype current_move_cost; /**< The cost of the selected move. */
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class State, class Move, typename CFtype>
MoveRunner<Input,State,Move,CFtype>::MoveRunner(const Input& in, 
		StateManager<Input,State,CFtype>& e_sm,
        NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
        const std::string& name)
  : Runner<Input,State,CFtype>(in, e_sm), ne(e_ne)
{
		EasyLocalObject::SetName(name);
}

/**
   Checks wether the object state is consistent with all the related
   objects.
*/
template <class Input, class State, class Move, typename CFtype>
void MoveRunner<Input,State,Move,CFtype>::Check() const
throw(EasyLocalException)
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
	this->current_state_cost += current_move_cost; 
}
#endif /*MOVERUNNER_HH_*/
