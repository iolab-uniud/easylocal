// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2008 Andrea Schaerf, Luca Di Gaspero. 
//
// EasyLocalpp is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// EasyLocalpp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with EasyLocalpp. If not, see <http://www.gnu.org/licenses/>.

#if !defined(_NEIGHBORHOOD_EXPLORER_HH_)
#define _NEIGHBORHOOD_EXPLORER_HH_

#include <helpers/DeltaCostComponent.hh>
#include <helpers/StateManager.hh>
#include <helpers/ProhibitionManager.hh>
#include <utils/Synchronize.hh>
#include <typeinfo>
#include <stdexcept>

#if defined(HAVE_PTHREAD)
#include <utils/Synchronize.hh>
#endif


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
typedef Move ThisMove;
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
 @throws EmptyNeighborhood when the State st has no neighbor
 */		
virtual CFtype FirstImprovingMove(const State& st, Move& mv) const;

/** 
 Generate the first improvement move in the exploration of the neighborhood
 of a given state.
 @param st the start state
 @param mv the generated move
 @throws EmptyNeighborhood when the State st has no neighbor
 */		
virtual CFtype FirstImprovingMove(const State& st, Move& mv, ProhibitionManager<State,Move,CFtype>& pm) const;
/** 
 Generates the best move in the full exploration of the neighborhood
 of a given state.
 @param st the start state.
 @param mv the generated move.
 @return the variation of the cost due to the Move mv.
 @throws EmptyNeighborhood when the State st has no neighbor 
 */
virtual CFtype BestMove(const State& st, Move& mv) const;
/** 
 Generates the best move in the full exploration of the neighborhood
 of a given state.
 @param st the start state.
 @param mv the generated move.
 @param pm a prohibition manager, which filters out prohibited moves (e.g., for the Tabu Search).
 @return the variation of the cost due to the Move mv.
 @throws EmptyNeighborhood when the State st has no neighbor 
 */
virtual CFtype BestMove(const State& st, Move& mv, ProhibitionManager<State, Move,CFtype>& pm) const;

/** 
 Generates a pair of moves in the full exploration of the neighborhood of a given state. The 
 moves are evaluated both according to their shifted value and their true cost and the best for each criterion is 
 generated.
 @param st the start state.
 @param shifted_mv the generated best move according to the shifted value.
 @param actual_mv the generated best move according to their true cost.
 @return a pair consisting of two shifted results: the shifted value cost for the @ref Move "Move"s shifted_mv and actual_mv, respectively.
 @throws EmptyNeighborhood when the State st has no neighbor
 */
virtual std::pair<ShiftedResult<CFtype>, ShiftedResult<CFtype> > BestShiftedMove(const State& st, Move& shifted_mv, Move& actual_mv) const;

/** 
 Generates a pair of moves in the full exploration of the neighborhood of a given state. The 
 moves are evaluated both according to their shifted value and their true cost and the best for each criterion is 
 generated.
 @param st the start state.
 @param shifted_mv the generated best move according to the shifted value.
 @param actual_mv the generated best move according to their true cost.
 @param pm a prohibition manager, which filters out prohibited moves (e.g., for the Tabu Search)
 @return a pair consisting of two shifted results: the shifted value cost for the @ref Move "Move"s shifted_mv and actual_mv, respectively.
 @throws EmptyNeighborhood when the State st has no neighbor
 */
virtual std::pair<ShiftedResult<CFtype>, ShiftedResult<CFtype> > BestShiftedMove(const State& st, Move& shifted_mv, Move& actual_mv, ProhibitionManager<State,Move,CFtype>& pm) const;



/** 
 Generates the best move in a sample exploration of the neighborhood
 of a given state.
 @param st the start state.
 @param mv the generated move.
 @param samples the number of sampled neighbors
 @return the variation of the cost due to the Move mv.
 @throws EmptyNeighborhood when the State st has no neighbor 
 */
virtual CFtype SampleMove(const State &st, Move& mv, unsigned int samples) const;

/** 
 Generates the best move in a sample exploration of the neighborhood
 of a given state.
 @param st the start state.
 @param mv the generated move.
 @param samples the number of sampled neighbors
 @param pm a prohibition manager, which filters out prohibited moves (e.g., for the Tabu Search).
 @return the variation of the cost due to the Move mv.
 @throws EmptyNeighborhood when the State st has no neighbor 
 */
virtual CFtype SampleMove(const State &st, Move& mv, unsigned int samples, ProhibitionManager<State,Move,CFtype>& pm) const;

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

virtual size_t DeltaCostComponents() const
{ return delta_cost_component.size(); }

virtual AbstractDeltaCostComponent<Input,State,Move,CFtype>& DeltaCostComponent(unsigned int i)
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
protected:
/** Checks wether an external request to terminate the exploration of the neighborhood 
 has been issued.
 */
  bool ExternalTerminationRequest() const;
#if defined(HAVE_PTHREAD)
RWLockVariable<bool> *p_external_termination_request;
public:
void SetExternalTerminationRequest(RWLockVariable<bool>& external_termination_request);
void ResetExternalTerminationRequest();
#endif
};



/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class State, class Move, typename CFtype>
NeighborhoodExplorer<Input,State,Move,CFtype>::NeighborhoodExplorer(const Input& i,
                                                                    StateManager<Input,State,CFtype>& e_sm, std::string e_name)
: in(i), sm(e_sm), number_of_delta_not_implemented(0), name(e_name)
{ 
#if defined(HAVE_PTHREAD)
  ResetExternalTerminationRequest();
#endif
}

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

template <class Input, class State, class Move, typename CFtype>
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::BestMove(const State &st, Move& mv) const
{
  unsigned int number_of_bests = 1; // number of moves found with the same best value
  FirstMove(st, mv);
  Move best_move = mv;
  CFtype mv_cost = DeltaCostFunction(st, mv);
  CFtype best_delta = mv_cost;
  
  while (NextMove(st, mv) && !ExternalTerminationRequest()) 
  { 		
    mv_cost = DeltaCostFunction(st, mv);
    if (LessThan(mv_cost, best_delta))
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
      }
    else if (EqualTo(mv_cost, best_delta))
      {
	if (Random::Int(0,number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
	  best_move = mv;
        number_of_bests++;
      } 
  }
  
  mv = best_move;
  return best_delta;
} 

template <class Input, class State, class Move, typename CFtype>
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::BestMove(const State &st, Move& mv, ProhibitionManager<State, Move, CFtype>& pm) const
{
  unsigned int number_of_bests = 1; // number of moves found with the same best value
  FirstMove(st,mv);
  CFtype mv_cost = DeltaCostFunction(st,mv);
  Move best_move = mv;
  CFtype best_delta = mv_cost;
  bool all_moves_prohibited = true, not_last_move;
  
  do // look for the best move 
  {  // if the prohibition mechanism is active get the best non-prohibited move
		// if all moves are prohibited, then get the best one
    if (LessThan(mv_cost, best_delta))
    {
      if (!pm.ProhibitedMove(st, mv, mv_cost))
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
        all_moves_prohibited = false;
      }
      if (all_moves_prohibited)
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
      }
    }
    else if (all_moves_prohibited && !pm.ProhibitedMove(st, mv, mv_cost))
    { // when the prohibition mechanism is active, even though it is not an improving move,
      // this move is the actual best since it is the first non-prohibited
      best_move = mv;
      best_delta = mv_cost;
      number_of_bests = 1;
      all_moves_prohibited = false;
    }
    else if (EqualTo(mv_cost, best_delta) && !pm.ProhibitedMove(st, mv, mv_cost))
    {
      if (Random::Int(0,number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
        best_move = mv;
      number_of_bests++;
    } 
    not_last_move = NextMove(st, mv);
    if (not_last_move)
      mv_cost = DeltaCostFunction(st, mv);
  }
  while (not_last_move && !ExternalTerminationRequest());
  mv = best_move;
  return best_delta;
}

template <class Input, class State, class Move, typename CFtype>
std::pair<ShiftedResult<CFtype>, ShiftedResult<CFtype> > NeighborhoodExplorer<Input,State,Move,CFtype>::BestShiftedMove(const State &st, Move& shifted_mv, Move& actual_mv) const 
{
  unsigned int number_of_shifted_bests = 1, number_of_actual_bests = 1; // number of moves found with the same best value
  Move mv;
  FirstMove(st, mv);
  ShiftedResult<CFtype> mv_cost = DeltaShiftedCostFunction(st, mv);
  Move best_shifted_move = mv, best_actual_move = mv;
  ShiftedResult<CFtype> best_shifted_delta = mv_cost, best_actual_delta = mv_cost;
  
  while (NextMove(st, mv) && !ExternalTerminationRequest()) 
  { 
		mv_cost = DeltaShiftedCostFunction(st, mv);
		if (LessThan(mv_cost.shifted_value, best_shifted_delta.shifted_value))
    {
			best_shifted_move = mv;
			best_shifted_delta = mv_cost;
			number_of_shifted_bests = 1;
    }
    else if (EqualTo(mv_cost.shifted_value, best_shifted_delta.shifted_value))
    {
      if (Random::Int(0, number_of_shifted_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
      {
        best_shifted_move = mv;
        best_shifted_delta = mv_cost;
      }
      number_of_shifted_bests++;
    } 
    // Search for the best actual move 
    if (LessThan(mv_cost.actual_value, best_actual_delta.actual_value))
    {
      best_actual_move = mv;
      best_actual_delta = mv_cost;
			number_of_actual_bests = 1;
    }    
		else if (EqualTo(mv_cost.actual_value, best_shifted_delta.actual_value))
		{
			if (Random::Int(0, number_of_actual_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
      {
        best_actual_move = mv;
        best_actual_delta = mv_cost;
      }
      number_of_actual_bests++;
		}
  }
  
  shifted_mv = best_shifted_move;
  actual_mv = best_actual_move;
  
  return std::pair<ShiftedResult<CFtype>, ShiftedResult<CFtype> >(best_shifted_delta, best_actual_delta);
} 

template <class Input, class State, class Move, typename CFtype>
std::pair<ShiftedResult<CFtype>, ShiftedResult<CFtype> > NeighborhoodExplorer<Input,State,Move,CFtype>::BestShiftedMove(const State &st, Move& shifted_mv, Move& actual_mv, ProhibitionManager<State,Move,CFtype>& pm) const 
{
  unsigned int number_of_shifted_bests = 1, number_of_actual_bests = 1; // number of moves found with the same best value
  Move mv;
  FirstMove(st, mv);
  ShiftedResult<CFtype> mv_cost = DeltaShiftedCostFunction(st, mv );
  Move best_shifted_move = mv, best_actual_move = mv;
  ShiftedResult<CFtype> best_shifted_delta = mv_cost, best_actual_delta = mv_cost;
  bool all_moves_prohibited = true, not_last_move;
  
  do // look for the best move 
  {  // if the prohibition mechanism is active get the best non-prohibited move
    // if all moves are prohibited, then get the best one
    if (LessThan(mv_cost.shifted_value, best_shifted_delta.shifted_value))
    {
      if (!pm.ProhibitedMove(st, mv, mv_cost.actual_value))
      {
        best_shifted_move = mv;
        best_shifted_delta = mv_cost;
        number_of_shifted_bests = 1;
        all_moves_prohibited = false;
      }
      if (all_moves_prohibited)
      {
        best_shifted_move = mv;
        best_shifted_delta = mv_cost;
        number_of_shifted_bests = 1;
      }
    }
    else if (all_moves_prohibited && !pm.ProhibitedMove(st, mv, mv_cost.actual_value))
    { // when the prohibition mechanism is active, even though it is not an improving move,
      // this move is the actual best since it is the first non-prohibited
      best_shifted_move = mv;
      best_shifted_delta = mv_cost;
      number_of_shifted_bests = 1;
      all_moves_prohibited = false;
    }
    else if (EqualTo(mv_cost.shifted_value, best_shifted_delta.shifted_value) && !pm.ProhibitedMove(st, mv, mv_cost.actual_value))
    {
      if (Random::Int(0, number_of_shifted_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
      {
        best_shifted_move = mv;
        best_shifted_delta = mv_cost;
      }
      number_of_shifted_bests++;
    } 
		// Search for the best actual move 
    if (LessThan(mv_cost.actual_value, best_actual_delta.actual_value))
    {
      best_actual_move = mv;
      best_actual_delta = mv_cost;
			number_of_actual_bests = 1;
    }    
		else if (EqualTo(mv_cost.actual_value, best_shifted_delta.actual_value) && !pm.ProhibitedMove(st, mv, mv_cost.actual_value))
		{
			if (Random::Int(0, number_of_actual_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
      {
        best_actual_move = mv;
        best_actual_delta = mv_cost;
      }
      number_of_actual_bests++;
		}
		not_last_move = NextMove(st, mv);
    if (not_last_move)
      mv_cost = DeltaShiftedCostFunction(st, mv);
  }
  while (not_last_move && !ExternalTerminationRequest());
  
  shifted_mv = best_shifted_move;
  actual_mv = best_actual_move;
  
  return std::pair<ShiftedResult<CFtype>, ShiftedResult<CFtype> >(best_shifted_delta, best_actual_delta);
}

template <class Input, class State, class Move, typename CFtype>
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::FirstImprovingMove(const State &st, Move& mv) const 
{
  unsigned int number_of_bests = 0;
  FirstMove(st, mv);
  CFtype mv_cost = DeltaCostFunction(st, mv);
  Move best_move = mv;
  CFtype best_delta = mv_cost; 
  
  while (NextMove(st, mv) && !ExternalTerminationRequest())
  {
		mv_cost = DeltaCostFunction(st, mv);
    if (LessThan(mv_cost, (CFtype)0))
			return mv_cost; // mv is an improving move

    if (LessThan(mv_cost, best_delta))
    {
			best_move = mv;
			best_delta = mv_cost;
			number_of_bests = 1;
    }
    else if (EqualTo(mv_cost, best_delta))
    {
      if (Random::Int(0,number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
        best_move = mv;
      number_of_bests++;
    }         
  }
  
  // these instructions are reached when no improving move has been found
  mv = best_move;
  return best_delta;
} 

template <class Input, class State, class Move, typename CFtype>
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::FirstImprovingMove(const State &st, Move& mv, ProhibitionManager<State,Move,CFtype>& pm) const 
{
  unsigned int number_of_bests = 0;
  FirstMove(st, mv);
  CFtype mv_cost = DeltaCostFunction(st, mv);
  Move best_move = mv;
  CFtype best_delta = mv_cost;
  bool all_moves_prohibited = true, not_last_move;
  
  do // look for the best move 
  {  // if the prohibition mechanism is active get the best non-prohibited move
		// if all moves are prohibited, then get the best one
		if (LessThan(mv_cost, (CFtype)0) && !pm.ProhibitedMove(st, mv, mv_cost))
			return mv_cost; // mv is an improving move
		
    if (LessThan(mv_cost, best_delta))
    {
      if (!pm.ProhibitedMove(st, mv, mv_cost))
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
        all_moves_prohibited = false;
      }
      if (all_moves_prohibited)
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
      }
    }
    else if (all_moves_prohibited && !pm.ProhibitedMove(st, mv, mv_cost))
    { // when the prohibition mechanism is active, even though it is not an improving move,
      // this move is the actual best since it is the first non-prohibited
      best_move = mv;
      best_delta = mv_cost;
      number_of_bests = 1;
      all_moves_prohibited = false;
    }
    else if (EqualTo(mv_cost, best_delta) && !pm.ProhibitedMove(st, mv, mv_cost))
    {
      if (Random::Int(0,number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
        best_move = mv;
      number_of_bests++;
    } 
    not_last_move = NextMove(st, mv);
    if (not_last_move)
      mv_cost = DeltaCostFunction(st, mv);
  }
  while (not_last_move && !ExternalTerminationRequest());

  // these instructions are reached when no improving move has been found  
  mv = best_move;
  return best_delta;
}


template <class Input, class State, class Move, typename CFtype>
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::SampleMove(const State &st, Move& mv, unsigned int samples) const
{
  unsigned int number_of_bests = 0;
  unsigned int s = 1;
  CFtype mv_cost;
  
  RandomMove(st, mv);
  mv_cost = DeltaCostFunction(st, mv);
  Move best_move = mv;
  CFtype best_delta = mv_cost;	
	
  do
  {
    if (LessThan(mv_cost, best_delta))
    {
			best_move = mv;
			best_delta = mv_cost;
			number_of_bests = 1;
    }
    else if (EqualTo(mv_cost, best_delta))
    {
      if (Random::Int(0,number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
        best_move = mv;
      number_of_bests++;
    }     
    RandomMove(st, mv);
    mv_cost = DeltaCostFunction(st, mv);
    s++;
  }
  while (s < samples && !ExternalTerminationRequest());
  
  mv = best_move;
  return best_delta;
}

template <class Input, class State, class Move, typename CFtype>
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::SampleMove(const State &st, Move& mv, unsigned int samples, ProhibitionManager<State,Move,CFtype>& pm) const
{
  unsigned int number_of_bests = 0;
  unsigned int s = 1;
  CFtype mv_cost;
  bool all_moves_prohibited = true;
  
  RandomMove(st, mv);
  mv_cost = DeltaCostFunction(st, mv);
  Move best_move = mv;
  CFtype best_delta = mv_cost;
  do
  {
    if (LessThan(mv_cost, best_delta))
    {
      if (!pm.ProhibitedMove(st, mv, mv_cost))
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
        all_moves_prohibited = false;
      }
      if (all_moves_prohibited)
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
      }
    }
    else if (all_moves_prohibited && !pm.ProhibitedMove(st, mv, mv_cost))
    { // when the prohibition mechanism is active, even though it is not an improving move,
      // this move is the actual best since it is the first non-prohibited
      best_move = mv;
      best_delta = mv_cost;
      number_of_bests = 1;
      all_moves_prohibited = false;
    }
    else if (EqualTo(mv_cost, best_delta) && !pm.ProhibitedMove(st, mv, mv_cost))
    {
      if (Random::Int(0,number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
        best_move = mv;
      number_of_bests++;
    }     
    RandomMove(st, mv);
    mv_cost = DeltaCostFunction(st, mv);
    s++;
  }
  while (s < samples && !ExternalTerminationRequest());
  
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

#if defined(HAVE_PTHREAD)
template <class Input, class State, class Move, typename CFtype>
void NeighborhoodExplorer<Input,State,Move,CFtype>::SetExternalTerminationRequest(RWLockVariable<bool>& external_termination_request) {
  p_external_termination_request = &external_termination_request;
}

template <class Input, class State, class Move, typename CFtype>
void NeighborhoodExplorer<Input,State,Move,CFtype>::ResetExternalTerminationRequest() {
  p_external_termination_request = NULL;
}
#endif

template <class Input, class State, class Move, typename CFtype>
inline bool NeighborhoodExplorer<Input,State,Move,CFtype>::ExternalTerminationRequest() const
{
#if defined(HAVE_PTHREAD)  
  if (p_external_termination_request)
    return *p_external_termination_request;
  else
    return false;
#else
  return false;
#endif
}

#endif // define _NEIGHBORHOOD_EXPLORER_HH_
