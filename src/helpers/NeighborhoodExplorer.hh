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
#include <utils/Random.hh>
#include <typeinfo>
#include <iostream>
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
  typedef Move ThisMove;


  typedef DeltaCostComponent<Input,State,Move,CFtype> DCC;
  typedef CostComponent<Input,State,CFtype> CC;

  /** Prints the configuration of the object (attached cost components)
      @param os Output stream 
  */
  void Print(std::ostream& os = std::cout) const;

  /** 
      Generates a random move in the neighborhood of a given state.	
      @note To be implemented in the application (MustDef)
      @param st the start state 
      @param mv the generated move 
  */
  virtual void RandomMove(const State &st, Move& mv) const = 0;

  /** Generates the first move in the neighborhood (a total ordering
      of the neighborhood is assumed). It is always used on
      cooperation with @ref NextMove to generate the whole
      neighborhood. It returns @c void because it is assumed that at
      least a move exists in the neighborhood.  It writes the first
      move in @c mv.
      
      @note To be implemented in the application (MustDef)
      @param st the start state 
      @param mv the move 
  */
  virtual void FirstMove(const State& st, Move& mv) const = 0;
  
  /** Generates the move that follows mv in the exploration of the
      neighborhood of the state st. 
      It returns the generated move in the same variable mv.
      @return @c false if @c mv is the last in the neighborhood of the state.
      
      @note To be implemented in the application.
      @param st the start state 
      @param mv the move 
  */
  virtual bool NextMove(const State &st, Move& mv) const = 0;

  /** 
      Generate the first improvingt move in the exploration of the neighborhood
      of a given state. It uses @ref FirstMove and @ref NextMove
      @param st the start state
      @param mv the generated move
      @throws EmptyNeighborhood when the State st has no neighbor
  */		
  virtual CFtype FirstImprovingMove(const State& st, Move& mv) const;

  /** 
      Generate the first improvement move in the exploration of the neighborhood
      of a given state, excluding the moves prohibited by @c pm.
      @param st the start state
      @param mv the generated move
      @param pm a prohibition manager, which filters out prohibited moves (e.g., for the Tabu Search).
      @throws EmptyNeighborhood when the State st has no neighbor
  */		
  virtual CFtype FirstImprovingMove(const State& st, Move& mv, ProhibitionManager<State,Move,CFtype>& pm) const;

  /** 
      Generates the best move in the full exploration of the neighborhood
      of a given state. It uses @ref FirstMove and @ref NextMove
      @param st the start state.
      @param mv the generated move.
      @return the variation of the cost due to the Move mv.
      @throws EmptyNeighborhood when the State st has no neighbor 
  */
  virtual CFtype BestMove(const State& st, Move& mv) const;

  /** 
      Generates the best move in the full exploration of the neighborhood
      of a given state, excluding the moves prohibited by @c pm.
      @param st the start state.
      @param mv the generated move.
      @param pm a prohibition manager, which filters out prohibited moves (e.g., for the Tabu Search).
      @return the variation of the cost due to the Move mv.
      @throws EmptyNeighborhood when the State st has no neighbor 
  */
  virtual CFtype BestMove(const State& st, Move& mv, ProhibitionManager<State,Move,CFtype>& pm) const;
  
  /** 
      Generates the best move in a random sample exploration of the neighborhood
      of a given state.
      @param st the start state.
      @param mv the generated move.
      @param samples the number of sampled neighbors
      @return the variation of the cost due to the Move mv.
      @throws EmptyNeighborhood when the State st has no neighbor 
  */
  virtual CFtype SampleMove(const State &st, Move& mv, unsigned int samples) const;

  /** 
      Generates the best move in a random sample exploration of the neighborhood
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
      By default it considers all the moves as feasible, but it can
      be overwritten by the user.
      
      @param st the start state
      @param @c mv the move checked for feasibility
      @return @c true if the move @mv is feasible in @c st, false otherwise
  */
  virtual bool FeasibleMove(const State& st, const Move& mv) const
  { 
    return true; 
  }

  /** 
      Modifies the state passed as parameter by applying a given
      move upon it.
      
      @note To be implemented in the application (MustDef)
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
    
  virtual void AddDeltaCostComponent(DCC& dcc);
  
  virtual void AddDeltaCostComponent(CC& cc);

  virtual size_t DeltaCostComponents() const
  { return delta_cost_component.size(); }
  
  virtual DeltaCostComponent<Input,State,Move,CFtype>& DeltaCostComponent(unsigned int i)
  { return *delta_cost_component[i]; }

  virtual size_t CostComponents() const
  { return cost_component.size(); }
  
  virtual CostComponent<Input,State,CFtype>& CostComponent(unsigned int i)
  { return *cost_component[i]; }
  
  virtual unsigned int Modality() const
  { return 1; }
  
  
  virtual unsigned int MoveModality(const Move& mv) const
  { return 0; }
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

  /** List of delta cost component */
  std::vector<DCC* > delta_cost_component;

  /** List of cost component */
  std::vector<CC* > cost_component;

  /** Name of user-defined neighborhood explorer */
  std::string name;
};

/*************************************************************************
 * Implementation
 *************************************************************************/


template <class Input, class State, class Move, typename CFtype>
NeighborhoodExplorer<Input,State,Move,CFtype>::NeighborhoodExplorer(const Input& i,
  StateManager<Input,State,CFtype>& e_sm, std::string e_name)
  : in(i), sm(e_sm), name(e_name)
{}

/**
 Evaluates the variation of the cost function obtainted either by applying the move to 
 the given state or simulating it.
 The tentative definition computes a weighted sum of the variation of 
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
  
  // compute delta costs
  for (i = 0; i < this->delta_cost_component.size(); i++)
  {
    // get reference to delta cost component
    DCC& dcc = *(delta_cost_component[i]);
    if (dcc.IsHard()) 
      delta_hard_cost += dcc.DeltaCost(st, mv);
    else
      delta_soft_cost += dcc.DeltaCost(st, mv);
  }

  // only if there is more than one cost component
  if (cost_component.size() != 0)
  {
    // compute move
    State st1 = st;
    MakeMove(st1, mv);
  
    for (i = 0; i < cost_component.size(); i++) 
    {
      // get reference to cost component
      CC& cc = *(cost_component[i]);
      if (cc.IsHard())
        // hard weight considered later
        delta_hard_cost += cc.ComputeCost(st1) - cc.ComputeCost(st);
      else
        delta_soft_cost += cc.Weight() * (cc.ComputeCost(st1) - cc.ComputeCost(st));
    }
  }
      			
  return HARD_WEIGHT * delta_hard_cost + delta_soft_cost;
}

template <class Input, class State, class Move, typename CFtype>
void NeighborhoodExplorer<Input,State,Move,CFtype>::AddDeltaCostComponent(DCC& dcc)
{
  delta_cost_component.push_back(&dcc);
}

template <class Input, class State, class Move, typename CFtype>
void NeighborhoodExplorer<Input,State,Move,CFtype>::AddDeltaCostComponent(CC& cc)
{
  cost_component.push_back(&cc);
}

template <class Input, class State, class Move, typename CFtype>
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::BestMove(const State &st, Move& mv) const
{
  unsigned int number_of_bests = 1; // number of moves found with the same best value

  FirstMove(st, mv);
  Move best_move = mv;
  CFtype mv_cost = DeltaCostFunction(st, mv);
  CFtype best_delta = mv_cost;
  
  while (NextMove(st, mv)) 
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
{ // get the best non-prohibited move, but if all moves are prohibited, then get the best one among them
  unsigned int number_of_bests = 1; // number of moves found with the same best value
  FirstMove(st,mv);
  CFtype mv_cost = DeltaCostFunction(st,mv);
  Move best_move = mv;
  CFtype best_delta = mv_cost;
  bool all_moves_prohibited = pm.ProhibitedMove(st, mv, mv_cost);

  static unsigned int i1 = 0, i2 = 0;
  
  while (NextMove(st, mv)) 
  { 		
    mv_cost = DeltaCostFunction(st, mv);
    if (LessThan(mv_cost, best_delta))
    {
      if (!pm.ProhibitedMove(st, mv, mv_cost))
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
        all_moves_prohibited = false;
      }
      else if (all_moves_prohibited)
      {
        best_move = mv;
        best_delta = mv_cost;
        number_of_bests = 1;
      }
    }
    else if (EqualTo(mv_cost, best_delta))
      {
	if (!pm.ProhibitedMove(st, mv, mv_cost))
	  {
	    if (all_moves_prohibited)
	      {
		best_move = mv;
		number_of_bests = 1;
		all_moves_prohibited = false;
	      }
	    else
	      {
		if (Random::Int(0,number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
		  best_move = mv;
		number_of_bests++;
	      }
	  }
	else 
	  if (all_moves_prohibited)
	    {
	      if (Random::Int(0,number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
		best_move = mv;
	      number_of_bests++;
	    }
      }
    else // mv_cost is greater than best_delta
      if (all_moves_prohibited && !pm.ProhibitedMove(st, mv, mv_cost))
	{
	  best_move = mv;
	  best_delta = mv_cost;
	  number_of_bests = 1;
	  all_moves_prohibited = false;
	}
  }

  if (all_moves_prohibited)
    i1++;
  i2++;
  //std::cerr << (float)i1/i2 << ' ' << best_move << " ";
  mv = best_move;
  return best_delta;
}

template <class Input, class State, class Move, typename CFtype>
CFtype NeighborhoodExplorer<Input,State,Move,CFtype>::FirstImprovingMove(const State &st, Move& mv) const 
{
  unsigned int number_of_bests = 0;
  FirstMove(st, mv);
  CFtype mv_cost = DeltaCostFunction(st, mv);
  Move best_move = mv;
  CFtype best_delta = mv_cost; 
  
  while (NextMove(st, mv))
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
  while (not_last_move);

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
  while (s < samples);
  
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
      DCC& dcc = *this->delta_cost_component[i];
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
      DCC& dcc = *this->delta_cost_component[i];
      total_delta += dcc.DeltaCost(st, mv);
    }
  return total_delta;
}

#endif // define _NEIGHBORHOOD_EXPLORER_HH_
