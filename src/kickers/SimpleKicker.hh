#ifndef SIMPLEKICKER_HH_
#define SIMPLEKICKER_HH_

#include "Kicker.hh"

/** A Simple Kicker simply considers sequences of moves of a given type
    (instantiated as template). 
    @ingroup Perturbers 
*/
template <class Input, class State, class Move, typename CFtype = int>
class SimpleKicker
  : public Kicker<Input,State,CFtype>
{
public:
  SimpleKicker(const Input& i, NeighborhoodExplorer<Input,State,Move>& nhe,                 
	       unsigned int s, std::string name);
  void Print(std::ostream& os = std::cout) const;
  CFtype SelectKick(const State& st);
  CFtype BestKick(const State &st);
  CFtype FirstImprovingKick(const State &st);
  CFtype DenseBestKick(const State &st);
  CFtype RandomKick(const State &st);
  CFtype TotalFirstImprovingKick(const State &st) { throw std::runtime_error("No TOTAL_BEST_KICK allowed for Simple Kickers"); }
  CFtype TotalBestKick(const State &st) { throw std::runtime_error("No TOTAL_FIRST_IMPROVING_KICK allowed for Simple Kickers"); }

  void MakeKick(State &st);
  virtual CFtype KickCost();
  virtual void PrintKick(std::ostream& os = std::cout) const;
  void SetMaxStep(unsigned int s);
protected:
  void FirstKick(const State &st) throw(KickerException);
  bool NextKick();
  Move GetKickComponent(unsigned int i) const;
  void SetKickComponent(unsigned int i, const Move& mv);
  //  virtual CFtype ComputeKickCost(const State &st);
  virtual bool RelatedMoves(const Move&, const Move&) const = 0;
  NeighborhoodExplorer<Input,State,Move>& ne;
  std::vector<Move> current_moves, internal_best_moves, start_moves;
  CFtype current_kick_cost, best_kick_cost;
  void FirstKickComponent(unsigned int i);  // used by the backtracking algorithm of bestkick
  bool NextKickComponent(unsigned int i);   // (idem)
  bool UnrelatedMoves(unsigned int i) const;      // (idem)
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class State, class Move, typename CFtype>
SimpleKicker<Input,State,Move,CFtype>::SimpleKicker(const Input& in,
						    NeighborhoodExplorer<Input,State,Move>& e_ne,
						    unsigned int s, std::string name)
  : Kicker<Input,State,CFtype>(in, s, name), ne(e_ne), current_moves(s), internal_best_moves(s), start_moves(s)
{}

template <class Input, class State, class Move, typename CFtype>
CFtype SimpleKicker<Input,State,Move,CFtype>::SelectKick(const State& st)
{
  switch (this->current_kick_type)
    {
    case RANDOM_KICK:
      return RandomKick(st);
    case BEST_KICK:
      return BestKick(st);
    case FIRST_IMPROVING_KICK:
      return FirstImprovingKick(st);
    case TOTAL_BEST_KICK:
      throw std::runtime_error("No TOTAL_BEST_KICK allowed for Simple Kickers");
      break;
    case TOTAL_FIRST_IMPROVING_KICK:
      throw std::runtime_error("No TOTAL_FIRST_IMPROVING_KICK allowed for Simple Kickers");
      break;
    default:
      throw std::runtime_error("Unknown Kick Type for Simple Kickers");    
    }
  return 0;  // Only to prevent warnings (never reached)
}

template <class Input, class State, class Move, typename CFtype>
void SimpleKicker<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
  os  << "Simple Kicker: " << this->GetName() << std::endl;
	
  os  << "Max Step: " << this->step << std::endl;
  os  << "Kick selection: ";
  switch (this->current_kick_type)
    {
    case RANDOM_KICK:
      os << "RANDOM" << std::endl;
      break;
    case BEST_KICK:
      os << "BEST" << std::endl;
      break;
    case TOTAL_BEST_KICK:
      os << "TOTAL BEST" << std::endl;
      break;
    case FIRST_IMPROVING_KICK:
      os << "FIRST_IMPROVING" << std::endl;
      break;
    case TOTAL_FIRST_IMPROVING_KICK:
      os << "TOTAL FIRST_IMPROVING" << std::endl;
      break;
    }
	
}

template <class Input, class State, class Move, typename CFtype>
void SimpleKicker<Input,State,Move,CFtype>::SetMaxStep(unsigned int s)
{
  Kicker<Input,State,CFtype>::SetMaxStep(s);
  current_moves.resize(s);
  internal_best_moves.resize(s);
  start_moves.resize(s);
}

template <class Input, class State, class Move, typename CFtype>
void SimpleKicker<Input,State,Move,CFtype>::FirstKickComponent(unsigned int i)
{
  ne.FirstMove(this->states[i], current_moves[i]);
  start_moves[i] = current_moves[i];
}

template <class Input, class State, class Move, typename CFtype>
bool SimpleKicker<Input,State,Move,CFtype>::NextKickComponent(unsigned int i)
{
  ne.NextMove(this->states[i], current_moves[i]);
  return current_moves[i] != start_moves[i];
  // Andrea 8-2-2002
  //return !ne.LastMoveDone(current_moves[i]);
}

template <class Input, class State, class Move, typename CFtype>
bool SimpleKicker<Input,State,Move,CFtype>::UnrelatedMoves(unsigned int i) const
{
  if (i == 0)
    return false;
  else
    return !RelatedMoves(current_moves[i-1], current_moves[i]);
}

template <class Input, class State, class Move, typename CFtype>
CFtype SimpleKicker<Input,State,Move,CFtype>::DenseBestKick(const State &st)
{
  unsigned dense_best_step = 0;
  CFtype dense_best_kick_cost = 0;
  vector<Move> dense_best_moves;
	
  for (this->step = 1; this->step <= this->max_step; this->step++)
    {
      BestKick(st);
      if (this->step == 1 || current_kick_cost < dense_best_kick_cost)
	{	  
	  dense_best_kick_cost = current_kick_cost;
	  dense_best_moves = current_moves;
	  dense_best_step = this->step;
	}
    }
  this->step = dense_best_step;
  current_kick_cost = dense_best_kick_cost;
  current_moves = dense_best_moves;
  return current_kick_cost;
}


template <class Input, class State, class Move, typename CFtype>
CFtype SimpleKicker<Input,State,Move,CFtype>::BestKick(const State &st)
{
  FirstKick(st);
  best_kick_cost = current_kick_cost;
  internal_best_moves = current_moves;
#if VERBOSE >= 3
  for (unsigned int j = 0; j < this->step; j++)
    std::cerr << j << " : " << internal_best_moves[j] << "   ";
  std::cerr << "first cost : " << best_kick_cost << std::endl;
#endif
  while (NextKick() && !this->Timeout())
    {
      if (LessThan(current_kick_cost,best_kick_cost))
	{
	  best_kick_cost = current_kick_cost;
	  internal_best_moves = current_moves;
#if VERBOSE >= 3
	  for (unsigned int j = 0; j < this->step; j++)
	    std::cerr << j << " : " << internal_best_moves[j] << "   ";
	  std::cerr << "best cost : " << best_kick_cost << std::endl;
#endif
	}
    }
  current_kick_cost = best_kick_cost;
  current_moves = internal_best_moves;
#if VERBOSE >= 3
  for (unsigned int j = 0; j < this->step; j++)
    std::cerr << j << " : " << internal_best_moves[j] << "   ";
  std::cerr << "final best cost : " << best_kick_cost << std::endl;
#endif
  return current_kick_cost;
}

template <class Input, class State, class Move, typename CFtype>
CFtype SimpleKicker<Input,State,Move,CFtype>::FirstImprovingKick(const State &st)
{
  FirstKick(st);
  best_kick_cost = current_kick_cost;
  internal_best_moves = current_moves;
#if VERBOSE >= 3
  for (unsigned int j = 0; j < this->step; j++)
    std::cerr << j << " : " << internal_best_moves[j] << "   ";
  std::cerr << "first cost : " << best_kick_cost << std::endl;
#endif
  if (LessThan(current_kick_cost,0)) return current_kick_cost;
  while (NextKick() && !this->Timeout())
    {
      if (LessThan(current_kick_cost,best_kick_cost))
	{
	  best_kick_cost = current_kick_cost;
	  internal_best_moves = current_moves;
#if VERBOSE >= 3
	  for (unsigned int j = 0; j < this->step; j++)
	    std::cerr << j << " : " << internal_best_moves[j] << "   ";
	  std::cerr << "best cost : " << best_kick_cost << std::endl;
#endif
	  if (LessThan(current_kick_cost,0)) return current_kick_cost;
	}
    }
  current_kick_cost = best_kick_cost;
  current_moves = internal_best_moves;
#if VERBOSE >= 3
  for (unsigned int j = 0; j < this->step; j++)
    std::cerr << j << " : " << internal_best_moves[j] << "   ";
  std::cerr << "final best cost : " << best_kick_cost << std::endl;
#endif
  return current_kick_cost;
}

template <class Input, class State, class Move, typename CFtype>
void SimpleKicker<Input,State,Move,CFtype>::FirstKick(const State &st) throw(KickerException)
{ 
  int i = 0;
  this->states[0] = st;
  FirstKickComponent(0);
  do
    {
      bool backtrack = UnrelatedMoves(i);
      if (i == int(this->step - 1) && !backtrack)
	{ // the first kick has been found
	  current_kick_cost = KickCost();
	  return;
	}
      if (backtrack)
	do
	  if (NextKickComponent(i))
	    backtrack = false;
	  else
	    i--;
	while (backtrack && i >= 0);
      else
	{
	  this->states[i+1] = this->states[i];
	  ne.MakeMove(this->states[i+1],current_moves[i]);
	  i++;
	  FirstKickComponent(i);
	}
    }
  while (i >= 0);
  throw KickerException("No kick build in SimpleKicker::FirstKick()");
}

template <class Input, class State, class Move, typename CFtype>
bool SimpleKicker<Input,State,Move,CFtype>::NextKick()
{
  int i(this->step - 1);
  bool backtrack = true;
  do
    {
      if (i == int(this->step - 1) && !backtrack)
	{
	  current_kick_cost = KickCost();
	  return true;
	}
      if (backtrack)
	do
	  if (NextKickComponent(i))
	    {
	      backtrack = false;
	    }
	  else
	    i--;
	while (backtrack && i >= 0);
      else
	{
	  this->states[i+1] = this->states[i];
	  ne.MakeMove(this->states[i+1],current_moves[i]);
	  i++;
	  FirstKickComponent(i);
	}
      backtrack = UnrelatedMoves(i);
    }
  while (i >= 0);
  return false;
}

template <class Input, class State, class Move, typename CFtype>
Move SimpleKicker<Input,State,Move,CFtype>::GetKickComponent(unsigned int i) const
{
  assert(i < this->step);
  return current_moves[i];
}

template <class Input, class State, class Move, typename CFtype>
void SimpleKicker<Input,State,Move,CFtype>::SetKickComponent(unsigned int i, const Move& mv)
{
  assert(i < this->step);
  current_moves[i] = mv;
}

template <class Input, class State, class Move, typename CFtype>
void SimpleKicker<Input,State,Move,CFtype>::MakeKick(State &st)
{
#if VERBOSE >= 3
  std::cerr << "Kick Made: ";
#endif
  for (unsigned int i = 0; i < this->step; i++)
    {
#if VERBOSE >= 3
      std::cerr << current_moves[i] << '[' << ne.DeltaCostFunction(st,current_moves[i]) << "] ";
#endif
      ne.MakeMove(st,current_moves[i]);
    }
}

template <class Input, class State, class Move, typename CFtype>
CFtype SimpleKicker<Input,State,Move,CFtype>::RandomKick(const State &st)
{
  this->states[0] = st;
  for (unsigned int i = 0; i < this->step; i++)
    {
      ne.RandomMove(this->states[i],current_moves[i]);
      this->states[i+1] = this->states[i];
      ne.MakeMove(this->states[i+1],current_moves[i]);
    }
  return KickCost();
}

template <class Input, class State, class Move, typename CFtype>
CFtype SimpleKicker<Input,State,Move,CFtype>::KickCost()
{
  CFtype cost = 0;
  for (unsigned int i = 0; i < this->step; i++) 
    cost += ne.DeltaCostFunction(this->states[i], current_moves[i]);
  return cost;
}

// template <class Input, class State, class Move, typename CFtype>
// CFtype SimpleKicker<Input,State,Move,CFtype>::ComputeKickCost(const State& st)
// {
//   CFtype cost = 0;
//   this->states[0] = st;
//   for (unsigned int i = 0; i < this->step; i++)
//     {
//       cost += ne.DeltaCostFunction(this->states[i], current_moves[i]);
//       this->states[i+1] = this->states[i];
//       ne.MakeMove(this->states[i+1], current_moves[i]);
//     }
//   return cost;
// }

template <class Input, class State, class Move, typename CFtype>
void SimpleKicker<Input,State,Move,CFtype>::PrintKick(std::ostream& os) const
{
  for (unsigned int i = 0; i < this->step; i++)
    os << current_moves[i] << '[' << ne.DeltaCostFunction(this->states[i],current_moves[i]) << "] ";		
  os << std::endl;
}

#endif /*SIMPLEKICKER_HH_*/
