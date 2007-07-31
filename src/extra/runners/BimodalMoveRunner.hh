#ifndef BIMODALMOVERUNNER_HH_
#define BIMODALMOVERUNNER_HH_

#include "../../helpers/StateManager.hh"
#include "../../helpers/NeighborhoodExplorer.hh"
#include "../../runners/Runner.hh"
#include "../../basics/EasyLocalException.hh"
#include <vector>

#ifndef MOVE_ENUM
typedef enum {
  MOVE_1 = 1,
  MOVE_2
  //    MOVE_3
} PatternMove;
#define MOVE_ENUM
#endif

typedef std::vector<PatternMove> PatternType;

/** A BiMove Runner is an instance of the Runner interface which it compels to
    with two particular definitions of @Move (given as template instantiations).
    It is at the root of the inheritance hierarchy of actual runners.
    @ingroup Runners
*/

template <class Input, class State, class Move1, class Move2, typename CFtype>
class BimodalMoveRunner
  : public Runner<Input,State,CFtype>
{
public:
  // Runner interface
  virtual void Check() const throw(EasyLocalException);
protected:
  BimodalMoveRunner(const Input& im, StateManager<Input,State,CFtype>& sm,
		    NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1,
		    NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2,
		    std::string name = "");
  /* state manipulations */
  virtual void GoCheck() const throw(EasyLocalException) = 0;
  /** Actions to be perfomed at the beginning of the run. */
  virtual void ComputeMoveCost();

  /** Encodes the criterion used to select the move at each step. */
  //    virtual void SelectMove() = 0;
  void MakeMove();
  void UpdateStateCost();
   

  NeighborhoodExplorer<Input,State,Move1>& ne1; /**< A pointer to the
						   attached neighborhood 
						   explorer. */
  NeighborhoodExplorer<Input,State,Move2>& ne2; /**< A pointer to the
						   attached neighborhood 
						   explorer. */
  // state data
  Move1 current_move1;      /**< The currently selected move. */
  Move2 current_move2;      /**< The currently selected move. */
  CFtype current_move_cost1; /**< The cost of the selected move. */
  CFtype current_move_cost2; /**< The cost of the selected move. */
  PatternMove current_move_type;
};

/*  ***** BIMOVE RUNNER ******* */

template <class Input, class State, class Move1, class Move2, typename CFtype>
BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::BimodalMoveRunner(const Input& in,
								     StateManager<Input,State,CFtype>& sm,
								     NeighborhoodExplorer<Input,State,Move1,CFtype>& e_ne1,
								     NeighborhoodExplorer<Input,State,Move2,CFtype>& e_ne2, std::string name)
  : Runner<Input,State,CFtype>(in, sm), ne1(e_ne1), ne2(e_ne2)
{ EasyLocalObject::SetName(name); }

/**
   Checks wether the object state is consistent with all the related
   objects.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::Check() const
  throw(EasyLocalException)
{
  Runner<Input,State,CFtype>::Check();
}

/**
   Actually performs the move selected by the local search strategy.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::MakeMove()
{
  if (this->current_move_type == MOVE_1)
    ne1.MakeMove(this->current_state, current_move1);
  else
    ne2.MakeMove(this->current_state, current_move2);
}


/**
   Computes the cost of the selected move; it delegates this task to the
   neighborhood explorer.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::ComputeMoveCost()
{
  if (this->current_move_type == MOVE_1)
    current_move_cost1 = ne1.DeltaCostFunction(this->current_state, current_move1);
  else
    current_move_cost2 = ne2.DeltaCostFunction(this->current_state, current_move2);
}

/**
   Updates the cost of the internal state of the runner.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::UpdateStateCost()
{
  if (this->current_move_type == MOVE_1)
    this->current_state_cost += current_move_cost1;
  else
    this->current_state_cost += current_move_cost2;
}

#endif /*BIMODALMOVERUNNER_HH_*/
