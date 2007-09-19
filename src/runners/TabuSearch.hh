#ifndef TABUSEARCH_HH_
#define TABUSEARCH_HH_

/** The Tabu Search runner explores a subset of the current
    neighborhood. Among the elements in it, the one that gives the
    minimum value of the cost function becomes the new current
    state, independently of the fact whether its value is less or
    greater than the current one.
    
    Such a choice allows the algorithm to escape from local minima,
    but creates the risk of cycling among a set of states.  In order to
    prevent cycling, the so-called tabu list is used, which
    determines the forbidden moves. This list stores the most recently
    accepted moves, and the inverses of the moves in the list are
    forbidden.  
    @ingroup Runners
*/

#include "MoveRunner.hh"
#include "../helpers/StateManager.hh"
#include "../helpers/NeighborhoodExplorer.hh"
#include "../helpers/TabuListManager.hh"
#include "../basics/EasyLocalException.hh"

template <class Input, class State, class Move, typename CFtype = int>
class TabuSearch
  : public MoveRunner<Input,State,Move,CFtype>
{
public:
  TabuSearch(const Input& in, StateManager<Input,State,CFtype>& e_sm,
	     NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
	     TabuListManager<State,Move,CFtype>& e_tlm,
	     const std::string& name = "Anonymous Tabu Search runner");

  void Print(std::ostream& os = std::cout) const;
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout)
    throw(EasyLocalException);
  virtual void SetMaxIdleIteration(unsigned long m) { max_idle_iteration = m; }
  TabuListManager<State,Move,CFtype>& GetTabuListManager() { return pm; }
  bool MaxIdleIterationExpired() const;
    protected:
  void GoCheck() const throw(EasyLocalException);
  void InitializeRun();
  bool StopCriterion();
  void SelectMove();
  bool AcceptableMove();
  void StoreMove();
  TabuListManager<State,Move,CFtype>& pm; /**< A reference to a tabu list manger. */
  // parameters
  unsigned long max_idle_iteration;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   Constructs a tabu search runner by linking it to a state manager, 
   a neighborhood explorer, a tabu list manager, and an input object.

   @param s a pointer to a compatible state manager
   @param ne a pointer to a compatible neighborhood explorer
   @param tlm a pointer to a compatible tabu list manager
   @param in a poiter to an input object
*/
template <class Input, class State, class Move, typename CFtype>
TabuSearch<Input,State,Move,CFtype>::TabuSearch(const Input& in,
						StateManager<Input,State,CFtype>& e_sm,
						NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
						TabuListManager<State,Move,CFtype>& tlm,
						const std::string& name)
  : MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name), pm(tlm), max_idle_iteration(0)
{}

template <class Input, class State, class Move, typename CFtype>
void TabuSearch<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
  os  << "Tabu Search Runner: " << this->GetName() << std::endl;
  os  << "  Max iterations: " << this->max_iteration << std::endl;
  os  << "  Max idle iteration: " << this->max_idle_iteration << std::endl;
  pm.Print(os);
}

/**
   Initializes the run by invoking the companion superclass method, and
   cleans the tabu list.
*/
template <class Input, class State, class Move, typename CFtype>
void TabuSearch<Input,State,Move,CFtype>::InitializeRun()
{
  MoveRunner<Input,State,Move,CFtype>::InitializeRun();
  pm.Clean();
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearch<Input,State,Move,CFtype>::GoCheck() const
  throw(EasyLocalException)
{
  if (this->max_idle_iteration == 0)
    throw EasyLocalException("max_idle_iteration is zero for object " + this->GetName());
}


/**
   Selects always the best move that is non prohibited by the tabu list 
   mechanism.
*/
template <class Input, class State, class Move, typename CFtype>
void TabuSearch<Input,State,Move,CFtype>::SelectMove()
{
  Move mv;
  register CFtype mv_cost;
  register bool all_moves_tabu = true;

  this->ne.FirstMove(this->current_state, mv);
  mv_cost = this->ne.DeltaCostFunction(this->current_state, mv);
  Move best_move = mv;
  CFtype best_delta = mv_cost;
  do  // look for the best non prohibited move
    {   // (if all moves are prohibited, then get the best)
        // For efficency, ProhibitedMove is invoked only when strictly necessary
      if (mv_cost < best_delta)
        {
	  if (!pm.ProhibitedMove(this->current_state, mv, mv_cost))
            {
	      best_move = mv;
	      best_delta = mv_cost;
	      all_moves_tabu = false;
            }
	  if (all_moves_tabu)
            {
	      best_move = mv;
	      best_delta = mv_cost;
            }
        }
      else if (all_moves_tabu && !pm.ProhibitedMove(this->current_state, mv, mv_cost))
        { // even though it is not an improving move,
	  // this move is the actual best since it's the first non-tabu
	  best_move = mv;
	  best_delta = mv_cost;
	  all_moves_tabu = false;
        }
      this->ne.NextMove(this->current_state, mv);
      mv_cost = this->ne.DeltaCostFunction(this->current_state, mv);
    }
  while (!this->ne.LastMoveDone(this->current_state, mv));

  this->current_move = best_move;
  this->current_move_cost = best_delta;

}

template <class Input, class State, class Move, typename CFtype>
bool TabuSearch<Input,State,Move,CFtype>::MaxIdleIterationExpired() const
{
  return this->number_of_iterations - this->iteration_of_best >= this->max_idle_iteration; 
}

/**
   The stop criterion is based on the number of iterations elapsed from
   the last strict improvement of the best state cost.
*/
template <class Input, class State, class Move, typename CFtype>
bool TabuSearch<Input,State,Move,CFtype>::StopCriterion()
{ 
  return MaxIdleIterationExpired() || this->MaxIterationExpired();
}

/**
   In tabu search the selected move is always accepted.
   That is, the acceptability test is replaced by the 
   prohibition mechanism which is managed inside the selection.
*/
template <class Input, class State, class Move, typename CFtype>
bool TabuSearch<Input,State,Move,CFtype>::AcceptableMove()
{ 
  return true; 
}

/**
   Stores the move by inserting it in the tabu list, if the state obtained
   is better than the one found so far also the best state is updated.
*/
template <class Input, class State, class Move, typename CFtype>
void TabuSearch<Input,State,Move,CFtype>::StoreMove()
{
#if VERBOSE >= 4
  std::cerr << "Move : " << this->current_move << ", Move Cost:" << this->current_move_cost << " (current: "
	    << this->current_state_cost << ", best: " 
	    << this->best_state_cost <<  ") it: " << this->number_of_iterations
	    << " (idle: " << this->number_of_iterations - this->iteration_of_best << ")" 
	    << "Costs: ";
  this->sm.PrintStateReducedCost(this->current_state, std::cerr);
  std::cerr << std::endl; 
#endif
  pm.InsertMove(this->current_state, this->current_move, this->current_move_cost,
		this->current_state_cost, this->best_state_cost);
  if (LessOrEqualThan(this->current_state_cost,this->best_state_cost))
    { // same cost states are accepted as best for diversification
      if (LessThan(this->current_state_cost,this->best_state_cost))
	{
#if VERBOSE == 3
	  std::cerr << "  New best: " << this->current_state_cost 
		    << " (it: " << this->number_of_iterations
		    << ", idle: " << this->number_of_iterations - this->iteration_of_best << "), " 
		    << "Costs: ";
	  this->sm.PrintStateReducedCost(this->current_state, std::cerr);
	  std::cerr << std::endl; 
#endif
	  this->iteration_of_best = this->number_of_iterations;
	  this->best_state_cost = this->current_state_cost;
	}
      this->best_state = this->current_state;
    }
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearch<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
  throw(EasyLocalException)
{
  os << "TABU SEARCH -- INPUT PARAMETERS" << std::endl;
  pm.ReadParameters(is, os);
  os << "  Number of idle iterations: ";
  is >> this->max_idle_iteration;
}
#endif /*TABUSEARCH_HH_*/
