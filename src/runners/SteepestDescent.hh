#ifndef STEEPESTDESCENT_HH_
#define STEEPESTDESCENT_HH_

#include "MoveRunner.hh"
#include "../helpers/StateManager.hh"
#include "../helpers/NeighborhoodExplorer.hh"
#include "../basics/EasyLocalException.hh"
#include "../utils/Types.hh"

/** The Steepest Descent runner performs a simple local search.
   At each step of the search, the best move in the neighborhood of current
   solution is selected and performed.
   It is worth noticing that this algorithm leads straightly to the 
   nearest local minimum of a given state.
   @ingroup Runners
*/  
template <class Input, class State, class Move, typename CFtype = int>
class SteepestDescent
            : public MoveRunner<Input,State,Move,CFtype>
{
public:
    SteepestDescent(const Input& in,
                    StateManager<Input,State,CFtype>& e_sm,
                    NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                    const std::string& name = "Anonymous Steepest Descent runner");
    void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout)
    throw(EasyLocalException);
    void Print(std::ostream& os = std::cout) const;
protected:
    void GoCheck() const throw(EasyLocalException);
    void InitializeRun();
    void TerminateRun();
    void StoreMove();
    bool StopCriterion();
    bool AcceptableMove();
    void SelectMove();
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   Constructs a steepest descent runner by linking it to a state manager, 
   a neighborhood explorer, and an input object.

   @param s a pointer to a compatible state manager
   @param ne a pointer to a compatible neighborhood explorer
   @param in a poiter to an input object
*/
template <class Input, class State, class Move, typename CFtype>
SteepestDescent<Input,State,Move,CFtype>::SteepestDescent(const Input& in,
        StateManager<Input,State,CFtype>& e_sm, NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
        const std::string& name)
        : MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name)
{}

template <class Input, class State, class Move, typename CFtype>
void SteepestDescent<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
    os  << "Steepest Descent Runner: " << this->GetName() << std::endl;
    os  << "  Max iterations: " << this->max_iteration << std::endl;
}

/**
   Selects always the best move in the neighborhood.
*/
template <class Input, class State, class Move, typename CFtype>
void SteepestDescent<Input,State,Move,CFtype>::SelectMove()
{
    this->current_move_cost = this->ne.BestMove(this->current_state, this->current_move);
}

/**
   Invokes the companion superclass method, and initializes the move cost
   at a negative value for fulfilling the stop criterion the first time
*/     
template <class Input, class State, class Move, typename CFtype>
void SteepestDescent<Input,State,Move,CFtype>::InitializeRun()
{
    MoveRunner<Input,State,Move,CFtype>::InitializeRun();
    this->current_move_cost = -1; // needed for passing the first time
    // the StopCriterion test
}

template <class Input, class State, class Move, typename CFtype>
void SteepestDescent<Input,State,Move,CFtype>::GoCheck() const
throw(EasyLocalException)
{}

/**
   The search is stopped when no (strictly) improving move has been found.
*/
template <class Input, class State, class Move, typename CFtype>
bool SteepestDescent<Input,State,Move,CFtype>::StopCriterion()
{ return GreaterOrEqualThan<CFtype>(this->current_move_cost,0); }

/**
   A move is accepted if it is an improving one.
*/
template <class Input, class State, class Move, typename CFtype>
bool SteepestDescent<Input,State,Move,CFtype>::AcceptableMove()
{ return LessThan<CFtype>(this->current_move_cost,0); }

template <class Input, class State, class Move, typename CFtype>
void SteepestDescent<Input,State,Move,CFtype>::StoreMove()
{
  if (LessThan(this->current_state_cost,this->best_state_cost))
    {
        this->iteration_of_best = this->number_of_iterations;
        this->best_state_cost = this->current_state_cost;
#if VERBOSE >= 2
	  std::cerr << "  New best: " << this->current_state_cost 
		    << " (it: " << this->number_of_iterations << "), " 
		    << "Costs: ";
	  this->sm.PrintStateReducedCost(this->current_state, std::cerr);
	  std::cerr << ", Move: " << this->current_move << std::endl; 	  
#endif
    }
}

/**
   At the end of the run, the best state found is set with the last visited
   state (it is always a local minimum).
*/
template <class Input, class State, class Move, typename CFtype>
void SteepestDescent<Input,State,Move,CFtype>::TerminateRun()
{
    MoveRunner<Input,State,Move,CFtype>::TerminateRun();
    this->best_state = this->current_state;
    this->best_state_cost = this->current_state_cost;
}

template <class Input, class State, class Move, typename CFtype>
void SteepestDescent<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
throw(EasyLocalException)
{}
#endif /*STEEPESTDESCENT_HH_*/
