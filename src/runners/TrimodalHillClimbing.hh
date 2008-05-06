#ifndef TRIMODALHILLCLIMBING_HH_
#define TRIMODALHILLCLIMBING_HH_

#include <TrimodalMoveRunner.hh>
#include <stdexcept>

/** The Hill Climbing runner considers random move selection. A move
   is then performed only if it does improve or it leaves unchanged
   the value of the cost function.  
   @ingroup Runners 
*/
template <class Input, class State, class Move1, class Move2, class Move3, typename CFtype = int>
class TrimodalHillClimbing
            : public TrimodalMoveRunner<Input,State,Move1,Move2,Move3,CFtype>
{
public:
    void Print(std::ostream& os = std::cout) const;
    void ReadParameters(std::istream& is = std::cin,
                        std::ostream& os = std::cout);
  TrimodalHillClimbing(const Input& in,
		       StateManager<Input,State,CFtype>& s,
		       NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1,
		       NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2,
		       NeighborhoodExplorer<Input,State,Move3,CFtype>& ne3,
                       std::string name);
    virtual void SetMaxIdleIteration(unsigned long m) { max_idle_iteration = m; }
protected:
    void GoCheck() const;
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
template <class Input, class State, class Move1, class Move2, class Move3, typename CFtype>
TrimodalHillClimbing<Input,State,Move1,Move2,Move3,CFtype>::TrimodalHillClimbing(const Input& in,
		       StateManager<Input,State,CFtype>& s,
		       NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1,
		       NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2,
		       NeighborhoodExplorer<Input,State,Move3,CFtype>& ne3,
                       std::string name)
  : TrimodalMoveRunner<Input,State,Move1,Move2,Move3,CFtype>(in, s, ne1, ne2, ne3, name), max_idle_iteration(0)
{}

template <class Input, class State, class Move1, class Move2, class Move3, typename CFtype>
void TrimodalHillClimbing<Input,State,Move1,Move2,Move3,CFtype>::Print(std::ostream& os) const
{
    os  << "TRIMODAL Hill Climbing Runner: " << this->GetName() << std::endl;
    os  << "  Max iterations: " << this->max_iteration << std::endl;
    os  << "  Max idle iteration: " << this->max_idle_iteration << std::endl;
}

/**
   The select move strategy for the hill climbing simply looks for a
   random move.
*/
template <class Input, class State, class Move1, class Move2, class Move3, typename CFtype>
void TrimodalHillClimbing<Input,State,Move1,Move2,Move3,CFtype>::SelectMove()
{
    this->ne1.RandomMove(this->current_state, this->current_move1);
    this->current_move_cost1 = this->ne1.DeltaCostFunction(this->current_state, this->current_move1);
    this->ne2.RandomMove(this->current_state, this->current_move2);
    this->current_move_cost2 = this->ne2.DeltaCostFunction(this->current_state, this->current_move2);
    this->ne3.RandomMove(this->current_state, this->current_move3);
    this->current_move_cost3 = this->ne3.DeltaCostFunction(this->current_state, this->current_move3);

    if (this->current_move_cost1 < this->current_move_cost2)
        if (this->current_move_cost1 < this->current_move_cost3)
            this->current_move_type = MOVE_1;
        else if (this->current_move_cost1 > this->current_move_cost3)
            this->current_move_type = MOVE_3;
        else
            this->current_move_type = Random::Int(0,1) == 0 ? MOVE_1 : MOVE_3;
    else if (this->current_move_cost1 > this->current_move_cost2)
        if (this->current_move_cost2 < this->current_move_cost3)
            this->current_move_type = MOVE_2;
        else if (this->current_move_cost2 > this->current_move_cost3)
            this->current_move_type = MOVE_3;
        else
            this->current_move_type = Random::Int(0,1) == 0 ? MOVE_2 : MOVE_3;
    else
        this->current_move_type = Random::Int(0,1) == 0 ? MOVE_1 : MOVE_2;
}

/**
   The hill climbing initialization simply invokes 
   the superclass companion method.
*/
template <class Input, class State, class Move1, class Move2, class Move3, typename CFtype>
void TrimodalHillClimbing<Input,State,Move1,Move2,Move3,CFtype>::InitializeRun()
{
    TrimodalMoveRunner<Input,State,Move1,Move2,Move3,CFtype>::InitializeRun();
}

template <class Input, class State, class Move1, class Move2, class Move3, typename CFtype>
void TrimodalHillClimbing<Input,State,Move1,Move2,Move3,CFtype>::GoCheck() const
{
    if (this->max_idle_iteration == 0)
			throw std::logic_error("max_idle_iteration is zero for object " + this->GetName());
}

/**
   At the end of the run, the best state found is set with the last visited
   state (it is always a local minimum).
*/
template <class Input, class State, class Move1, class Move2, class Move3, typename CFtype>
void TrimodalHillClimbing<Input,State,Move1,Move2,Move3,CFtype>::TerminateRun()
{
    TrimodalMoveRunner<Input,State,Move1,Move2,Move3,CFtype>::TerminateRun();
    this->best_state = this->current_state;
    this->best_state_cost = this->current_state_cost;
}

/**
   The stop criterion for the hill climbing strategy is based on the number
   of iterations elapsed from the last strict improving move performed.
*/
template <class Input, class State, class Move1, class Move2, class Move3, typename CFtype>
bool TrimodalHillClimbing<Input,State,Move1,Move2,Move3,CFtype>::StopCriterion()
{ return this->number_of_iterations - this->iteration_of_best >= this->max_idle_iteration; }

/**
   A move is accepted if it is non worsening (i.e., it improves the cost
   or leaves it unchanged).
*/
template <class Input, class State, class Move1, class Move2, class Move3, typename CFtype>
bool TrimodalHillClimbing<Input,State,Move1,Move2,Move3,CFtype>::AcceptableMove()
{
    switch (this->current_move_type)
    {
    case MOVE_1:
        return this->current_move_cost1 <= 0;
    case MOVE_2:
        return this->current_move_cost2 <= 0;
    case MOVE_3:
        return this->current_move_cost3 <= 0;
    }
    return false;
}

/**
   The store move for hill climbing simply updates the variable that
   keeps track of the last improvement.
*/
template <class Input, class State, class Move1, class Move2, class Move3, typename CFtype>
void TrimodalHillClimbing<Input,State,Move1,Move2,Move3,CFtype>::StoreMove()
{
    bool update_state = false;
    switch (this->current_move_type)
    {
    case MOVE_1:
      if (LessThan(this->current_move_cost1,0))
        {
            update_state = true;
            //      std::cerr << 1 << ' ' << this->current_move_cost1 << std::endl;
        }
        break;
    case MOVE_2:
      if (LessThan(this->current_move_cost2,0))
        {
            update_state = true;
            //          std::cerr << 2 << ' ' << this->current_move_cost2 << std::endl;
        }
        break;
    case MOVE_3:
      if (LessThan(this->current_move_cost3,0))
        {
            update_state = true;
            //          std::cerr << 3 << ' ' << this->current_move_cost3 << std::endl;
        }
        break;
    }
    if (update_state)
    {
        this->iteration_of_best = this->number_of_iterations;
        this->best_state_cost = this->current_state_cost;
    }
}

template <class Input, class State, class Move1, class Move2, class Move3, typename CFtype>
void TrimodalHillClimbing<Input,State,Move1,Move2,Move3,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
    os << "TRIMODAL HILL CLIMBING -- INPUT PARAMETERS" << std::endl;
    os << "  Number of idle iterations: ";
    is >> this->max_idle_iteration;
    os << "  Timeout: ";
    is >> this->timeout;
}

#endif /*TRIMODALHILLCLIMBING_HH_*/
