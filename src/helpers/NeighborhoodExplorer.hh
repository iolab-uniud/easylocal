#ifndef NEIGHBORHOODEXPLORER_HH_
#define NEIGHBORHOODEXPLORER_HH_

#include "DeltaCostComponent.hh"
#include "StateManager.hh"
#include <typeinfo>
#include <stdexcept>

/** The Neighborhood Explorer is responsible for the strategy
    exploited in the exploration of the neighborhood, and for 
    computing the variations of the cost function due to a specific
    @c Move. 
    @ingroup Helpers
*/


template <class Input, class State, class Move, typename CFtype = int>
class NeighborhoodExplorer
{
public:
  
  void Print(std::ostream& os = std::cout) const;
  
  // move generating functions
  virtual void FirstMove(const State& st, Move& mv);
  
  /** Generates the move that follows mv in the exploration of the
      neighborhood of the state st. 
      It returns the generated move in the same variable mv.
    
      @note @bf To be implemented in the application.
      @param st the start state 
      @param mv the move 
  */
  virtual void NextMove(const State &st, Move& mv) = 0;
  
  /** Generates a random move in the neighborhood of a given state.
   * 
   * @note @bf To be implemented in the application.
   * @param st the start state 
   * @param mv the generated move 
   */
  virtual void RandomMove(const State &st, Move& mv) = 0;
  
  /** Generates the best move in the full exploration of the neighborhood
   *  of a given state.
   * @param st the start state
   * @param mv the generated move
   */
  virtual CFtype BestMove(const State& st, Move& mv);
		
  /** Generate the first improvement move in the exploration of the neighborhood
   *  of a given state.
   * @param st the start state
   * @param mv the generated move
   */		
  virtual CFtype FirstImprovingMove(const State& st, Move& mv);
		
  virtual CFtype SampleMove(const State &st, Move& mv, unsigned int samples);
  
  // end of exploration detection
  virtual bool LastMoveDone(const State&st, const Move &mv) const;
  
  /** States whether a move is feasible or not in a given state.
      For default it acceptsall the moves as feasible ones, but it can
      be overwritten by the user.
    
      @param st the start state
      @param mv the move to check for feasibility
      @return true if the move is feasible in st, false otherwise
  */
  virtual bool FeasibleMove(const State&, const Move&)
  { return true; }
  
  /** Modifies the state passed as parameter by applying a given
      move upon it.
    
      @note @bf To be implemented in the application.
      @param st the state to modify
      @param mv the move to be applied
  */
  virtual void MakeMove(State &st, const Move& mv) = 0;
  
  // evaluation function
  virtual CFtype DeltaCostFunction(const State& st, const Move& mv);
  virtual CFtype DeltaObjective(const State& st, const Move & mv);
  virtual CFtype DeltaViolations(const State& st, const Move & mv);
  //virtual CFtype DeltaObjective(const State& st, const State& st1, const Move & mv);
  //virtual CFtype DeltaViolations(const State& st, const State& st1, const Move & mv);
  
  virtual ShiftedResult<CFtype> DeltaShiftedCostFunction(const State& st, const Move& mv);
  //virtual ShiftedResult<CFtype> DeltaShiftedObjective(const State& st, const Move & mv);
  //virtual ShiftedResult<CFtype> DeltaShiftedViolations(const State& st, const Move & mv);
  //virtual ShiftedResult<CFtype> DeltaShiftedObjective(const State& st, const State& st1, const Move & mv);
  //virtual ShiftedResult<CFtype> DeltaShiftedViolations(const State& st, const State& st1, const Move & mv);
  
  virtual void AddDeltaCostComponent(AbstractDeltaCostComponent<Input,State,Move,CFtype>& dcc);
  
  virtual unsigned int DeltaCostComponents()
  { return delta_cost_component.size(); }
  
  virtual AbstractDeltaCostComponent<Input,State,Move,CFtype>& DeltaCostComponent(unsigned i)
  { return *delta_cost_component[i]; }
  
  // debugging/statistic functions
  //  virtual void PrintNeighborhoodStatistics(const State &st, std::ostream& os = std::cout);
//   virtual void ReadMove(Move& mv, std::istream& is = std::cin);
//   virtual void PrintMoveInfo(const State &st, const Move& mv, std::ostream& os = std::cout);
  virtual void PrintMoveCost(const State &st, const Move& mv, std::ostream& os = std::cout);
  
  /** Prompts for reading a move in the neighborhood of a given state
      from an input stream.
    
      @param st the start state
      @param mv the move read from the input stream
      @param is the input stream
  */
  virtual void InputMove(const State &st, Move& mv,
                         std::istream& is = std::cin) const;
  
  void Check() const;
protected:
		NeighborhoodExplorer(const Input& in, StateManager<Input,State,CFtype>& sm, std::string name);
  virtual ~NeighborhoodExplorer() {}
  
  const Input& in;/**< A reference to the input manager */
  StateManager<Input, State,CFtype>& sm; /**< A reference to the attached state manager. */
    
  Move best_move; /**< The best move found in the exploration of the
		     neighborhood (used from the neighborhood enumerating
		     functions such as BestMove). */
    
  Move start_move;  /**< The first move in the exploration of
		       the neighborhood. Needed to detect the end of the exploration. */
    
  std::vector<AbstractDeltaCostComponent<Input,State,Move,CFtype>* > delta_cost_component;
  unsigned number_of_delta_not_implemented;
  std::string name;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   Constructs a neighborhood explorer passing a pointer to a state manager 
   and a pointer to the input.
 
   @param sm a pointer to a compatible state manager
   @param in a pointer to an input object virtual void InputMove(const State &st, Move& mv, 
   std::istream& is = std::cin) const 
*/
template <class Input, class State, class Move, typename CFtype>
NeighborhoodExplorer<Input,State,Move,CFtype>::NeighborhoodExplorer(const Input& i,
                                                                    StateManager<Input,State,CFtype>& e_sm, std::string e_name)
  : in(i), sm(e_sm), delta_cost_component(0), number_of_delta_not_implemented(0), name(e_name)
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
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaCostFunction(const State& st, const Move & mv)
{
  CFtype delta_hard_cost = 0, delta_soft_cost = 0;
  
  if (number_of_delta_not_implemented == 0)
    {
      for (unsigned int i = 0; i < delta_cost_component.size(); i++)
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
      for (unsigned int i = 0; i < delta_cost_component.size(); i++)
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
ShiftedResult<CFtype> NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaShiftedCostFunction(const State& st, const Move & mv)
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
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::BestMove(const State &st, Move& mv)
{
  FirstMove(st,mv);
  CFtype mv_cost = DeltaCostFunction(st,mv);
  best_move = mv;
  CFtype best_delta = mv_cost;
  do // look for the best move
    {
      mv_cost = DeltaCostFunction(st,mv);
      if (mv_cost < best_delta)
	{
	  best_move = mv;
	  best_delta = mv_cost;
	}
      NextMove(st,mv);
    }
  while (!LastMoveDone(st,mv));
  mv = best_move;
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
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::FirstImprovingMove(const State &st, Move& mv)
{
  FirstMove(st,mv);
  CFtype mv_cost = DeltaCostFunction(st,mv);
  best_move = mv;
  CFtype best_delta = mv_cost;
  do // look for the best move
    {
      mv_cost = DeltaCostFunction(st,mv);
      if (mv_cost < best_delta)
	{
	  best_move = mv;
	  best_delta = mv_cost;
	}
      NextMove(st,mv);
    }
  while (!LastMoveDone(st,mv) && mv_cost >= 0.0);
  mv = best_move;
  return best_delta;
} 

template <class Input, class State, class Move, typename CFtype>
void NeighborhoodExplorer<Input,State,Move,CFtype>::InputMove(const State &,
                                                              Move&,
                                                              std::istream&) const
{ throw std::logic_error("The function InputMove() has not been redefined"
                           " for the NeighborhoodExplorer" + name); }

/**
   Generates the first move in the exploration of the neighborhood of a 
   given state. 
   By default, it invokes the RandomMove function and records mv as 
   start move.
 
   @param st the state
*/
template <class Input, class State, class Move, typename CFtype>
void NeighborhoodExplorer<Input,State,Move,CFtype>::FirstMove(const State& st, Move& mv)
{
  RandomMove(st,mv);
  start_move = mv;
}

/**
   Returns the best move found out of a number of sampled moves from a given
   state.
 
   @param st the state
   @param mv the best move found
   @param samples the number of sampled moves
   @return the cost of the move mv
*/
template <class Input, class State, class Move, typename CFtype>
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::SampleMove(const State &st, Move& mv, unsigned int samples)
{
  unsigned int s = 1;
  RandomMove(st,mv);
  CFtype mv_cost = DeltaCostFunction(st,mv);
  best_move = mv;
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
   Outputs the state cost components of the state passed as parameter.
 
   @param st the state to be inspected
*/
template <class Input, class State, class Move, typename CFtype>
void NeighborhoodExplorer<Input,State,Move,CFtype>::PrintMoveCost(const State& st,
                                                                  const Move& mv,
                                                                  std::ostream& os)
{ 
  CFtype delta_cost, total_delta_hard_cost = 0, total_delta_soft_cost = 0;
  if (number_of_delta_not_implemented > 0)
    {
      State st1 = st;
      MakeMove(st1,mv);
    
      for (unsigned i = 0; i < delta_cost_component.size(); i++)
	{
	  os << "  " << i << ". " << delta_cost_component[i]->name << " : ";
	  if (delta_cost_component[i]->IsDeltaImplemented())
	    {
	      FilledDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<FilledDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
	      delta_cost = dcc.DeltaCost(st, mv);        
	    }
	  else
	    {
	      EmptyDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<EmptyDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
	      delta_cost = dcc.DeltaCost(st, st1);
	    }
	  os <<  delta_cost;
	  if (delta_cost_component[i]->IsHard())
	    {
	      total_delta_hard_cost += delta_cost;
	      os << '*';
	    }
	  else
	    total_delta_soft_cost += delta_cost;
	  os << std::endl;
	}
        
    }
  else
    {
      for (unsigned i = 0; i < delta_cost_component.size(); i++)
	{
	  FilledDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<FilledDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
	  os << "  " << i << ". " << dcc.name << " : ";
	  delta_cost = dcc.DeltaCost(st, mv);
	  os << delta_cost;
	  if (delta_cost_component[i]->IsHard())
	    {
	      total_delta_hard_cost += delta_cost;
	      os << '*';
	    }
	  else
	    total_delta_soft_cost += delta_cost;
	  os << std::endl;
	}
    }
  os << "Total Delta Violations : " << total_delta_hard_cost << std::endl;
  os << "Total Delta Objective : " << total_delta_soft_cost << std::endl;
  os << "Total Delta Cost : " << HARD_WEIGHT * total_delta_hard_cost + total_delta_soft_cost << std::endl;
}

/**
   Checks wether the object state is consistent with all the related
   objects.
*/
template <class Input, class State, class Move, typename CFtype>
void NeighborhoodExplorer<Input,State,Move,CFtype>::Check() const
{}

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
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaViolations(const State& st, const Move & mv)
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
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaObjective(const State& st, const Move & mv)
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

/* template <class Input, class State, class Move, typename CFtype>
   CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaObjective(const State& st, const State& st1, const Move & mv)
   {
   CFtype total_delta = 0;
   for (unsigned i = 0; i < this->delta_cost_component.size(); i++)
   if (!delta_cost_component[i]->IsHard())
   if (this->delta_cost_component[i]->IsDeltaImplemented())
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

/* template <class Input, class State, class Move, typename CFtype>
   ShiftedResult<CFtype> NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaShiftedViolations(const State& st, const Move & mv)
   {
   // in the current version, if hard_delta_cost is not used
   // the function returns 0. This means that the default
   // version is not available
   ShiftedResult<CFtype> total_delta;
   for (unsigned i = 0; i < delta_cost_component.size(); i++)
   if (delta_cost_component[i]->IsHard())
   {
   FilledDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<FilledDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
   total_delta = total_delta + dcc.DeltaShiftedCost(st, mv);
   }
   return total_delta;
   }

   template <class Input, class State, class Move, typename CFtype>
   ShiftedResult<CFtype> NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaShiftedViolations(const State& st, const State& st1, const Move & mv)
   {
   ShiftedResult<CFtype> total_delta;
   for (unsigned i = 0; i < delta_cost_component.size(); i++)
   if (delta_cost_component[i]->IsHard())
   if (delta_cost_component[i]->IsDeltaImplemented())
   {
   FilledDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<FilledDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
   total_delta = total_delta + dcc.DeltaShiftedCost(st, mv);
   }
   else
   {
   EmptyDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<EmptyDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
   total_delta = total_delta + dcc.DeltaShiftedCost(st, st1);
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
/* template <class Input, class State, class Move, typename CFtype>
   ShiftedResult<CFtype> NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaShiftedObjective(const State& st, const Move & mv)
   {
   ShiftedResult<CFtype> total_delta;
   for (unsigned i = 0; i < this->delta_cost_component.size(); i++)
   if (!delta_cost_component[i]->IsHard())
   {
   FilledDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<FilledDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
   total_delta = total_delta + dcc.DeltaShiftedCost(st, mv);
   }
   return total_delta;
   }

   template <class Input, class State, class Move, typename CFtype>
   ShiftedResult<CFtype> NeighborhoodExplorer<Input,State,Move,CFtype>::DeltaShiftedObjective(const State& st, const State& st1, const Move & mv)
   {
   ShiftedResult<CFtype> total_delta;
   for (unsigned i = 0; i < this->delta_cost_component.size(); i++)
   if (!delta_cost_component[i]->IsHard())
   if (delta_cost_component[i]->IsDeltaImplemented())
   {
   FilledDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<FilledDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
   total_delta = total_delta + dcc.DeltaShiftedCost(st, mv);
   }
   else
   {
   EmptyDeltaCostComponent<Input,State,Move,CFtype>& dcc = static_cast<EmptyDeltaCostComponent<Input,State,Move,CFtype>& >(*this->delta_cost_component[i]);
   total_delta = total_delta + dcc.DeltaShiftedCost(st, st1);
   }
   return total_delta;
   } */
/**
   Checks whether the whole neighborhood has been explored.
   The tentative definition verifies is the move passed as parameter 
   coincides with the start move.
 
   @param mv the move to check
   @return true if the whole neighborhood has been explored, false otherwise
*/
template <class Input, class State, class Move, typename CFtype>
bool NeighborhoodExplorer<Input,State,Move,CFtype>::LastMoveDone(const State& st,
                                                                 const Move& mv) const
{ return mv == start_move; }

#endif /*NEIGHBORHOODEXPLORER_HH_*/
