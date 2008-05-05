#ifndef TRIMODALMOVERUNNER_HH_
#define TRIMODALMOVERUNNER_HH_

/** A TriMove Runner is an instance of the Runner interface which it compels to
  with three particular definitions of @Move (given as template instantiations).
  It is at the root of the inheritance hierarchy of actual runners.
  @ingroup Runners
*/
template <class Input, class State, class Move1, class Move2, class Move3, typename CFtype = int>
class TrimodalMoveRunner
            : public Runner<Input,State,CFtype>
{
public:
    // Runner interface
    virtual void Check();
    void SetInput(Input* in);
    // event functions, needed by observers (defined below)
protected:
    /* state manipulations */
    virtual void GoCheck() const = 0;
    /** Actions to be perfomed at the beginning of the run. */
    virtual void ComputeMoveCost();

    /** Encodes the criterion used to select the move at each step. */
    //    virtual void SelectMove() = 0;
    void MakeMove();
    void UpdateStateCost();
    TrimodalMoveRunner(const Input& in,
		       StateManager<Input,State,CFtype>& sm,
		       NeighborhoodExplorer<Input,State,Move1,CFtype>& e_ne1,
		       NeighborhoodExplorer<Input,State,Move2,CFtype>& e_ne2,
		       NeighborhoodExplorer<Input,State,Move3,CFtype>& e_ne3, std::string name);
    /** Virtual destructor. */

    NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1; /**< A pointer to the
                  attached neighborhood 
                  explorer. */
    NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2; /**< A pointer to the
                  attached neighborhood 
                  explorer. */
    NeighborhoodExplorer<Input,State,Move3,CFtype>& ne3; /**< A pointer to the
                  attached neighborhood 
                  explorer. */
    // state data
    Move1 current_move1;      /**< The currently selected move. */
    Move2 current_move2;      /**< The currently selected move. */
    Move3 current_move3;      /**< The currently selected move. */
    CFtype current_move_cost1; /**< The cost of the selected move. */
    CFtype current_move_cost2; /**< The cost of the selected move. */
    CFtype current_move_cost3; /**< The cost of the selected move. */
    PatternMove current_move_type;
};

// TRIMOVE RUNNER

template <class Input, class State, class Move1, class Move2, class Move3, typename CFtype>
TrimodalMoveRunner<Input,State,Move1,Move2,Move3,CFtype>::TrimodalMoveRunner(const Input& in,
								      StateManager<Input,State,CFtype>& sm,
        NeighborhoodExplorer<Input,State,Move1,CFtype>& e_ne1,
        NeighborhoodExplorer<Input,State,Move2,CFtype>& e_ne2,
        NeighborhoodExplorer<Input,State,Move3,CFtype>& e_ne3, std::string name)
        : Runner<Input,State,CFtype>(in,sm,name), ne1(e_ne1), ne2(e_ne2), ne3(e_ne3)
{}

/**
   Actually performs the move selected by the local search strategy.
 */
template <class Input, class State, class Move1, class Move2, class Move3, typename CFtype>
void TrimodalMoveRunner<Input,State,Move1,Move2,Move3,CFtype>::MakeMove()
{
    switch (this->current_move_type)
    {
    case MOVE_1:
        ne1.MakeMove(this->current_state, current_move1);
        break;
    case MOVE_2:
        ne2.MakeMove(this->current_state, current_move2);
        break;
    case MOVE_3:
        ne3.MakeMove(this->current_state, current_move3);
        break;
    }
}


/**
   Computes the cost of the selected move; it delegates this task to the
   neighborhood explorer.
*/
template <class Input, class State, class Move1, class Move2, class Move3, typename CFtype>
void TrimodalMoveRunner<Input,State,Move1,Move2,Move3,CFtype>::ComputeMoveCost()
{
    switch (this->current_move_type)
    {
    case MOVE_1:
        current_move_cost1 = ne1.DeltaCostFunction(this->current_state, current_move1);
        break;
    case MOVE_2:
        current_move_cost2 = ne2.DeltaCostFunction(this->current_state, current_move2);
        break;
    case MOVE_3:
        current_move_cost3 = ne3.DeltaCostFunction(this->current_state, current_move3);
        break;
    }
}

/**
   Updates the cost of the internal state of the runner.
*/
template <class Input, class State, class Move1, class Move2, class Move3, typename CFtype>
void TrimodalMoveRunner<Input,State,Move1,Move2,Move3,CFtype>::UpdateStateCost()
{
    switch (this->current_move_type)
    {
    case MOVE_1:
        this->current_state_cost += current_move_cost1;
        break;
    case MOVE_2:
        this->current_state_cost += current_move_cost2;
        break;
    case MOVE_3:
        this->current_state_cost += current_move_cost3;
        break;
    }
}

#endif /*TRIMODALMOVERUNNER_HH_*/
