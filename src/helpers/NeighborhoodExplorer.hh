#ifndef _NEIGHBORHOOD_EXPLORER_HH_
#define _NEIGHBORHOOD_EXPLORER_HH_

#include <helpers/DeltaCostComponent.hh>
#include <helpers/StateManager.hh>
#include <typeinfo>
#include <stdexcept>

class EmptyNeighborhood : public std::exception {};

/** The Neighborhood Explorer is responsible for the strategy
    exploited in the exploration of the neighborhood, and for 
    computing the variations of the cost function due to a specific
    @ref Move. 
 
    @ingroup Helpers
*/

template <class Input, class State, class Move, typename CFtype = int>
class NeighborhoodExplorer
{
public:   
  void Print(std::ostream& os = std::cout) const;
  /** 
	 Generates a random move in the neighborhood of a given state.	
   @note To be implemented in the application.
   @param st the start state 
   @param mv the generated move 
   */
  virtual void RandomMove(const State &st, Move& mv) const = 0;
  // move generating functions
  virtual void FirstMove(const State& st, Move& mv) const = 0;
  
  /** Generates the move that follows mv in the exploration of the
      neighborhood of the state st. 
      It returns the generated move in the same variable mv.
      Returns false if mv is the last in the state.
    
      @note To be implemented in the application.
      @param st the start state 
      @param mv the move 
  */
  virtual bool NextMove(const State &st, Move& mv) const = 0;

		
  /** 
	 Generate the first improvement move in the exploration of the neighborhood
   of a given state.
   @param st the start state
   @param mv the generated move
   */		
  virtual CFtype FirstImprovingMove(const State& st, Move& mv) const throw (EmptyNeighborhood);
  /** 
	 Generates the best move in the full exploration of the neighborhood
	 of a given state.
   @param st the start state
   @param mv the generated move
   */
  virtual CFtype BestMove(const State& st, Move& mv) const throw (EmptyNeighborhood);
		
  virtual CFtype SampleMove(const State &st, Move& mv, unsigned int samples) const throw (EmptyNeighborhood);
  
//   // end of exploration detection
//   virtual bool LastMoveDone(const State&st, const Move &mv) const;
  
  /** 
	 States whether a move is feasible or not in a given state.
	 By default it acceptsall the moves as feasible ones, but it can
	 be overwritten by the user.
    
	 @param st the start state
	 @param mv the move to check for feasibility
	 @return true if the move is feasible in st, false otherwise
  */
  virtual bool FeasibleMove(const State& st, const Move& mv) const
  { return true; }
  
  /** 
	 Modifies the state passed as parameter by applying a given
	 move upon it.
	 
	 @note To be implemented in the application.
	 @param st the state to modify
	 @param mv the move to be applied
  */
  virtual void MakeMove(State &st, const Move& mv) const = 0;
  
  virtual bool NextRelatedMove(const State &st, Move& mv, const Move& mv2) const
  { return NextMove(st,mv); }
  virtual bool FirstRelatedMove(const State &st, Move& mv, const Move& mv2) const 
  {
    try 
      {
	FirstMove(st,mv); 
      }
    catch (EmptyNeighborhood e)
      {
	return false;
      }
    return true;
  }

  // evaluation function
  virtual CFtype DeltaCostFunction(const State& st, const Move& mv) const;
  virtual CFtype DeltaObjective(const State& st, const Move & mv) const;
  virtual CFtype DeltaViolations(const State& st, const Move & mv) const;
  
  virtual ShiftedResult<CFtype> DeltaShiftedCostFunction(const State& st, const Move& mv) const;
  
  virtual void AddDeltaCostComponent(AbstractDeltaCostComponent<Input,State,Move,CFtype>& dcc);
  
  virtual unsigned int DeltaCostComponents() const
  { return delta_cost_component.size(); }
  
  virtual AbstractDeltaCostComponent<Input,State,Move,CFtype>& DeltaCostComponent(unsigned i)
  { return *delta_cost_component[i]; }
  
  
protected:
/**
 Constructs a neighborhood explorer passing a pointer to a state manager 
 and a pointer to the input.
 
 @param in a pointer to an input object.
 @param sm a pointer to a compatible state manager.
 @param name the name associated to the NeighborhoodExplorer.
 */
NeighborhoodExplorer(const Input& in, StateManager<Input,State,CFtype>& sm, std::string name);
  virtual ~NeighborhoodExplorer() {}
  

  const Input& in;/**< A reference to the input manager */
  StateManager<Input, State,CFtype>& sm; /**< A reference to the attached state manager. */
    
//   Move best_move; /**< The best move found in the exploration of the
// 		     neighborhood (used from the neighborhood enumerating
// 		     functions such as BestMove). */
    
//   Move start_move;  /**< The first move in the exploration of
// 		       the neighborhood. Needed to detect the end of the exploration. */
    
  std::vector<AbstractDeltaCostComponent<Input,State,Move,CFtype>* > delta_cost_component;
  unsigned number_of_delta_not_implemented;
  std::string name;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class State, class Move, typename CFtype>
NeighborhoodExplorer<Input,State,Move,CFtype>::NeighborhoodExplorer(const Input& i,
                                                                    StateManager<Input,State,CFtype>& e_sm, std::string e_name)
  : in(i), sm(e_sm), number_of_delta_not_implemented(0), name(e_name)
{ }

/**
   Evaluates the variation of the cost function obtainted by applying the
   move to the given state.
   The tentative definition computes a shifted sum of the variation of 
   the violations function and of the difference in the objective function.
 
   @param st the start state
   @param mv the move
   @return the variation in the cost function
*/
template <class Input, class State, class Move, typename CFtype>
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaCostFunction(const State& st, const Move & mv) const
{
  CFtype delta_hard_cost = 0, delta_soft_cost = 0;
  unsigned int i;

  if (number_of_delta_not_implemented == 0)
    {
      for (i = 0; i < delta_cost_component.size(); i++)
	{
	  FilledDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<FilledDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
	  if (dcc.IsHard())
	    delta_hard_cost += dcc.DeltaCost(st, mv);
	  else
	    delta_soft_cost += dcc.DeltaCost(st, mv);			
	}
    }
  else
    {
      State st1 = st;
      MakeMove(st1, mv);
      for (i = 0; i < delta_cost_component.size(); i++)
	if (delta_cost_component[i]->IsHard())
	  if (delta_cost_component[i]->IsDeltaImplemented())
	    {
	      FilledDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<FilledDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
	      delta_hard_cost += dcc.DeltaCost(st, mv);
	    }
	  else
	    {
	      EmptyDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<EmptyDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
	      delta_hard_cost += dcc.DeltaCost(st, st1);
	    }
	else
	  if (delta_cost_component[i]->IsDeltaImplemented())
	    {
	      FilledDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<FilledDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
	      delta_soft_cost += dcc.DeltaCost(st, mv);
	    }
	  else
	    {
	      EmptyDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<EmptyDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
	      delta_soft_cost += dcc.DeltaCost(st, st1);
	    }					
    }
  return HARD_WEIGHT * delta_hard_cost + delta_soft_cost;
}

template <class Input, class State, class Move, typename CFtype>
ShiftedResult<CFtype> NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaShiftedCostFunction(const State& st, const Move & mv) const
{
  ShiftedResult<CFtype> delta_hard_cost, delta_soft_cost;
  
  if (number_of_delta_not_implemented == 0)
    {
      for (unsigned int i = 0; i < delta_cost_component.size(); i++)
	{
	  FilledDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<FilledDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
	  if (dcc.IsHard())
	    delta_hard_cost = delta_hard_cost + dcc.DeltaShiftedCost(st, mv);
	  else
	    delta_soft_cost = delta_soft_cost + dcc.DeltaShiftedCost(st, mv);			
	}
    }
  else
    {
      State st1 = st;
      MakeMove(st1, mv);
      for (unsigned int i = 0; i < delta_cost_component.size(); i++)
	if (delta_cost_component[i]->IsHard())
	  if (delta_cost_component[i]->IsDeltaImplemented())
	    {
	      FilledDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<FilledDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
	      delta_hard_cost = delta_hard_cost + dcc.DeltaShiftedCost(st, mv);
	    }
	  else
	    {
	      EmptyDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<EmptyDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
	      delta_hard_cost = delta_hard_cost + dcc.DeltaShiftedCost(st, st1);
	    }
	else
	  if (delta_cost_component[i]->IsDeltaImplemented())
	    {
	      FilledDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<FilledDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
	      delta_soft_cost = delta_soft_cost + dcc.DeltaShiftedCost(st, mv);
	    }
	  else
	    {
	      EmptyDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<EmptyDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
	      delta_soft_cost = delta_soft_cost + dcc.DeltaShiftedCost(st, st1);
	    }					
    }
  return HARD_WEIGHT * delta_hard_cost + delta_soft_cost;
}

template <class Input, class State, class Move, typename CFtype>
void NeighborhoodExplorer<Input,State,Move,CFtype>::AddDeltaCostComponent(AbstractDeltaCostComponent<Input,State,Move,CFtype>& dcc)
{
  delta_cost_component.push_back(&dcc);
  if (!dcc.IsDeltaImplemented())
    number_of_delta_not_implemented++;
}

/**
   Looks for the best move in the exploration of the neighborhood of a given 
   state. (i.e., the one that gives the best improvement in the cost 
   function).
 
   @param st the state
   @param mv the best move in the state st
   @return the cost of move mv
*/
template <class Input, class State, class Move, typename CFtype>
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::BestMove(const State &st, Move& mv) const throw (EmptyNeighborhood)
{
  unsigned int number_of_bests = 1; // number of moves found with the same best value
  FirstMove(st,mv);
  CFtype mv_cost = DeltaCostFunction(st,mv);
  Move best_move = mv;
  CFtype best_delta = mv_cost;
  do // look for the best move
    {
      mv_cost = DeltaCostFunction(st,mv);
      if (LessThan(mv_cost, best_delta))
	{
	  best_move = mv;
	  best_delta = mv_cost;
	  number_of_bests = 1;	  
	}
      else if (EqualTo(mv_cost, best_delta))
	{
	  if (Random::Int(0,number_of_bests) == 0) // with probability 1/(1+number_of_bests)
	    best_move = mv;
	  number_of_bests++;
	} 
    }
  while (NextMove(st,mv));
  mv = best_move;
  std::cerr << number_of_bests << std::endl;
  return best_delta;
} 

/**
   Looks for the best move in the exploration of the neighborhood of a given 
   state. (i.e., the one that gives the best improvement in the cost 
   function).
 
   @param st the state
   @param mv the best move in the state st
   @return the cost of move mv
*/
template <class Input, class State, class Move, typename CFtype>
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::FirstImprovingMove(const State &st, Move& mv) const throw (EmptyNeighborhood)
{
  FirstMove(st,mv);
  CFtype mv_cost = DeltaCostFunction(st,mv);
  Move best_move = mv;
  CFtype best_delta = mv_cost;
  do // look for the best move
    {
      mv_cost = DeltaCostFunction(st,mv);
      if (LessThan(mv_cost, best_delta))
	{
	  best_move = mv;
	  best_delta = mv_cost;
	}
    }
  while (NextMove(st,mv) && GreaterOrEqualThan(mv_cost,(CFtype)0));
  mv = best_move;
  return best_delta;
} 


// /**
//    Generates the first move in the exploration of the neighborhood of a 
//    given state. 
//    By default, it invokes the RandomMove function and records mv as 
//    start move.
 
//    @param st the state
// */
// template <class Input, class State, class Move, typename CFtype>
// void NeighborhoodExplorer<Input,State,Move,CFtype>::FirstMove(const State& st, Move& mv)
// {
//   RandomMove(st,mv);
//   start_move = mv;
// }

// template <class Input, class State, class Move, typename CFtype>
// void NeighborhoodExplorer<Input,State,Move,CFtype>::FirstRelatedMove(const State& st, Move& mv, const Move& mv_r)
// {
//   RandomRelatedMove(st,mv,mv_r);
//   start_move = mv;
// }

/**
   Returns the best move found out of a number of sampled moves from a given
   state.
 
   @param st the state
   @param mv the best move found
   @param samples the number of sampled moves
   @return the cost of the move mv
*/
template <class Input, class State, class Move, typename CFtype>
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::SampleMove(const State &st, Move& mv, unsigned int samples) const throw (EmptyNeighborhood)
{
  unsigned int s = 1;
  RandomMove(st,mv);
  CFtype mv_cost = DeltaCostFunction(st,mv);
  Move best_move = mv;
  CFtype best_delta = mv_cost;
  do // look for the best sampled move
    {
      mv_cost = DeltaCostFunction(st,mv);
      if (mv_cost < best_delta)
	{
	  best_move = mv;
	  best_delta = mv_cost;
	}
      RandomMove(st,mv);
      s++;
    }
  while (s < samples);
  mv = best_move;
  return best_delta;
}



/**
   Evaluates the variation of the violations function obtained by 
   performing a move in a given state.
   The tentative definition simply makes the move and invokes the 
   companion StateManager method (Violations) on the initial and on the
   final state.
 
   @param st the state
   @param mv the move to evaluate
   @return the difference in the violations function induced by the move mv
*/
template <class Input, class State, class Move, typename CFtype>
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaViolations(const State& st, const Move & mv) const
{
  // in the current version, if hard_delta_cost is not used
  // the function returns 0. This means that the default
  // version is not available
  CFtype total_delta = 0;
  for (unsigned i = 0; i < delta_cost_component.size(); i++)
    if (delta_cost_component[i]->IsHard())
      {
	FilledDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<FilledDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
	total_delta += dcc.DeltaCost(st, mv);
      }
  return total_delta;
}

/* template <class Input, class State, class Move, typename CFtype>
   CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaViolations(const State& st, const State& st1, const Move & mv)
   {
   CFtype total_delta = 0;
   for (unsigned i = 0; i < delta_cost_component.size(); i++)
   if (delta_cost_component[i]->IsHard())
   if (delta_cost_component[i]->IsDeltaImplemented())
   {
   FilledDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<FilledDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
   total_delta += dcc.DeltaCost(st, mv);
   }
   else
   {
   EmptyDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<EmptyDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
   total_delta += dcc.DeltaCost(st, st1);
   }
   return total_delta;
   } */

/**
   Evaluates the variation of the objective function obtained by performing
   a move in a given state.
   The tentative definition simply makes the move and invokes the 
   companion StateManager method (Objective) on the initial and on the
   final state.
 
   @param st the state
   @param mv the move to evaluate
   @return the difference in the objective function induced by the move mv
*/
template <class Input, class State, class Move, typename CFtype>
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaObjective(const State& st, const Move & mv) const
{
  CFtype total_delta = 0;
  for (unsigned i = 0; i < this->delta_cost_component.size(); i++)
    if (delta_cost_component[i]->IsSoft())
      {
	FilledDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<FilledDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
	total_delta += dcc.DeltaCost(st, mv);
      }
  return total_delta;
}


// /**
//    Checks whether the whole neighborhood has been explored.
//    The tentative definition verifies is the move passed as parameter 
//    coincides with the start move.
 
//    @param mv the move to check
//    @return true if the whole neighborhood has been explored, false otherwise
// */
// template <class Input, class State, class Move, typename CFtype>
// bool NeighborhoodExplorer<Input,State,Move,CFtype>::LastMoveDone(const State& st,
//                                                                  const Move& mv) const
// { return mv == start_move; }

#endif // define _NEIGHBORHOOD_EXPLORER_HH_
