#ifndef _BIMODAL_FIRST_DESCENT_HH_
#define _BIMODAL_FIRST_DESCENT_HH_

#include <runners/BimodalMoveRunner.hh>

/** The First Descent runner ...  
    @ingroup Runners 
*/
template <class Input, class State, class Move1, class Move2, typename CFtype = int>
class BimodalFirstDescent
            : public BimodalMoveRunner<Input,State,Move1,Move2,CFtype>
{
public:
    void Print(std::ostream& os = std::cout) const;
	void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
    BimodalFirstDescent(const Input& in,
			   StateManager<Input,State,CFtype>& sm,
                        NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1,
                        NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2,
                        std::string name = "Anonymous Bimodal First Descent runner");
protected:
    void GoCheck() const;
    void InitializeRun();
    void TerminateRun();
    bool StopCriterion();
    bool AcceptableMove();
    void StoreMove();
    void SelectMove();
    // no parameters
};

/*************************************************************************
 * Implementation
 *************************************************************************/
 
/**
   Constructs a first descent runner by linking it to a state manager, 
   a neighborhood explorer, and an input object.

   @param s a pointer to a compatible state manager
   @param ne a pointer to a compatible neighborhood explorer
   @param in a poiter to an input object
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
BimodalFirstDescent<Input,State,Move1,Move2,CFtype>::BimodalFirstDescent(const Input& in,
									StateManager<Input,State,CFtype>& sm,
									NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1,
									NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2,
									std::string name)
        : BimodalMoveRunner<Input,State,Move1,Move2,CFtype>(in, sm, ne1, ne2, name)
{}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalFirstDescent<Input,State,Move1,Move2,CFtype>::Print(std::ostream& os) const
{
    os  << "First Descent Runner: " << this->name << std::endl;
    os  << "  Max iterations: " << this->max_iteration << std::endl;
}

/**
   The select move strategy for the first descent simply looks for a
   random move.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalFirstDescent<Input,State,Move1,Move2,CFtype>::SelectMove()
{    
  this->current_move_cost1 = this->ne1.FirstImprovingMove(this->current_state, this->current_move1);
  if (LessThan(this->current_move_cost1,CFtype(0)))
    this->current_move_type = MOVE_1;
  else
    {
      this->current_move_cost2 = this->ne2.FirstImprovingMove(this->current_state, this->current_move2);
      this->current_move_type = MOVE_2;
    }
}

/**
   The first descent initialization simply invokes 
   the superclass companion method.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalFirstDescent<Input,State,Move1,Move2,CFtype>::InitializeRun()
{
    BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::InitializeRun();
    this->current_move_cost1 = -1;     // needed for passing the first time the StopCriterion test
    this->current_move_type = MOVE_1;  

}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalFirstDescent<Input,State,Move1,Move2,CFtype>::GoCheck() const
{
}

/**
   At the end of the run, the best state found is set with the last visited
   state (it is always a local minimum).
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalFirstDescent<Input,State,Move1,Move2,CFtype>::TerminateRun()
{
    BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::TerminateRun();
    this->best_state = this->current_state;
    this->best_state_cost = this->current_state_cost;
}

/**
   The stop criterion for the first descent strategy is based on the number
   of iterations elapsed from the last strict improving move performed.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
bool BimodalFirstDescent<Input,State,Move1,Move2,CFtype>::StopCriterion()
{ 
  if (this->current_move_type == MOVE_1)
    return GreaterOrEqualThan(this->current_move_cost1, 0);
  else
    return GreaterOrEqualThan(this->current_move_cost2, 0);
}


/**
   A move is accepted if it is non worsening (i.e., it improves the cost
   or leaves it unchanged).
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
bool BimodalFirstDescent<Input,State,Move1,Move2,CFtype>::AcceptableMove()
{
  if (this->current_move_type == MOVE_1)
    return LessThan(this->current_move_cost1,0);
  else
    return LessThan(this->current_move_cost2,0);
}

/**
   The store move for first descent simply updates the variable that
   keeps track of the last improvement.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalFirstDescent<Input,State,Move1,Move2,CFtype>::StoreMove()
{
  if (this->observer != NULL)
    this->observer->NotifyNewBest(*this);
  if (this->current_move_type == MOVE_1)
    if (LessThan(this->current_move_cost1,0))
      {
	this->iteration_of_best = this->number_of_iterations;
	this->best_state_cost = this->current_state_cost;
      }
    else
      if (LessThan(this->current_move_cost2,0))
	{
	  this->iteration_of_best = this->number_of_iterations;
	  this->best_state_cost = this->current_state_cost;
	}
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalFirstDescent<Input,State,Move1,Move2,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
    os << "FIRST DESCENT -- INPUT PARAMETERS" << std::endl;
    os << "none: ";
}
#endif // define _BIMODAL_FIRST_DESCENT_HH_
