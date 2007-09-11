#ifndef BIMODALHILLCLIMBING_HH_
#define BIMODALHILLCLIMBING_HH_

#include "BimodalMoveRunner.hh"

/** The Hill Climbing runner considers random move selection. A move
    is then performed only if it does improve or it leaves unchanged
    the value of the cost function.  
    @ingroup Runners 
*/
template <class Input, class State, class Move1, class Move2, typename CFtype = int>
class BimodalHillClimbing
            : public BimodalMoveRunner<Input,State,Move1,Move2,CFtype>
{
public:
    void Print(std::ostream& os = std::cout) const;
    void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout)
    throw(EasyLocalException);
    virtual void SetMaxIdleIteration(unsigned long m) { max_idle_iteration = m; }
    BimodalHillClimbing(const Input& in,
    					StateManager<Input,State,CFtype>& sm,
                        NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1,
                        NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2,
                        std::string name = "Anonymous Bimodal Hill Climbing runner");
protected:
    void GoCheck() const throw(EasyLocalException);
    void InitializeRun();
    void TerminateRun();
    bool StopCriterion();
    bool AcceptableMove();
    void StoreMove();
    void SelectMove();
    // parameters
    unsigned long max_idle_iteration;
};

/*************************************************************************
 * Implementation
 *************************************************************************/
 
/**
   Constructs a hill climbing runner by linking it to a state manager, 
   a neighborhood explorer, and an input object.

   @param s a pointer to a compatible state manager
   @param ne a pointer to a compatible neighborhood explorer
   @param in a poiter to an input object
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
BimodalHillClimbing<Input,State,Move1,Move2,CFtype>::BimodalHillClimbing(const Input& in,
																																	StateManager<Input,State,CFtype>& sm,
																																	NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1,
																																	NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2,
																																	std::string name)
        : BimodalMoveRunner<Input,State,Move1,Move2,CFtype>(in, sm, ne1, ne2, name), max_idle_iteration(0)
{}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalHillClimbing<Input,State,Move1,Move2,CFtype>::Print(std::ostream& os) const
{
    os  << "Hill Climbing Runner: " << this->GetName() << std::endl;
    os  << "  Max iterations: " << this->max_iteration << std::endl;
    os  << "  Max idle iteration: " << this->max_idle_iteration << std::endl;
}

/**
   The select move strategy for the hill climbing simply looks for a
   random move.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalHillClimbing<Input,State,Move1,Move2,CFtype>::SelectMove()
{
    this->ne1.RandomMove(this->current_state, this->current_move1);
    this->current_move_cost1 = this->ne1.DeltaCostFunction(this->current_state, this->current_move1);
    this->ne2.RandomMove(this->current_state, this->current_move2);
    this->current_move_cost2 = this->ne2.DeltaCostFunction(this->current_state, this->current_move2);
    if (LessThan(this->current_move_cost1, this->current_move_cost2))
        this->current_move_type = MOVE_1;
    else if (this->current_move_cost1 > this->current_move_cost2)
        this->current_move_type = MOVE_2;
    else
        this->current_move_type = Random::Int(0,1) == 0 ? MOVE_1 : MOVE_2;
}

/**
   The hill climbing initialization simply invokes 
   the superclass companion method.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalHillClimbing<Input,State,Move1,Move2,CFtype>::InitializeRun()
{
    BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::InitializeRun();
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalHillClimbing<Input,State,Move1,Move2,CFtype>::GoCheck() const
throw(EasyLocalException)
{
    if (this->max_idle_iteration == 0)
        throw EasyLocalException("this->max_idle_iteration is zero for object " + this->GetName());
}

/**
   At the end of the run, the best state found is set with the last visited
   state (it is always a local minimum).
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalHillClimbing<Input,State,Move1,Move2,CFtype>::TerminateRun()
{
    BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::TerminateRun();
    this->best_state = this->current_state;
    this->best_state_cost = this->current_state_cost;
}

/**
   The stop criterion for the hill climbing strategy is based on the number
   of iterations elapsed from the last strict improving move performed.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
bool BimodalHillClimbing<Input,State,Move1,Move2,CFtype>::StopCriterion()
{ return this->number_of_iterations - this->iteration_of_best >= this->max_idle_iteration; }

/**
   A move is accepted if it is non worsening (i.e., it improves the cost
   or leaves it unchanged).
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
bool BimodalHillClimbing<Input,State,Move1,Move2,CFtype>::AcceptableMove()
{
    if (this->current_move_type == MOVE_1)
        return this->current_move_cost1 <= 0;
    else
        return this->current_move_cost2 <= 0;
}

/**
   The store move for hill climbing simply updates the variable that
   keeps track of the last improvement.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalHillClimbing<Input,State,Move1,Move2,CFtype>::StoreMove()
{
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
void BimodalHillClimbing<Input,State,Move1,Move2,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
throw(EasyLocalException)
{
    os << "HILL CLIMBING -- INPUT PARAMETERS" << std::endl;
    os << "  Number of idle iterations: ";
    is >> this->max_idle_iteration;
    os << "  Timeout: ";
    is >> this->timeout;
}
#endif /*BIMODALHILLCLIMBING_HH_*/
