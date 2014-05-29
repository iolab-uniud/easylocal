#if !defined(_SIMPLE_KICKER_HH_)
#define _SIMPLE_KICKER_HH_

#include <stdexcept>

#include "kickers/Kicker.hh"
#include "observers/SimpleKickerObserver.hh"

/** A Simple Kicker simply considers sequences of moves of a given type
    (instantiated as template). 
    @ingroup Perturbers 
*/
template <class Input, class State, class Move, typename CFtype>
class SimpleKicker
  : public Kicker<Input,State,CFtype>
{
  friend class SimpleKickerObserver<Input,State,Move,CFtype>;
public:
  SimpleKicker(const Input& i, NeighborhoodExplorer<Input,State,Move,CFtype>& nhe,                 
	       unsigned int s, std::string name);
  void Print(std::ostream& os = std::cout) const;
  void AttachObserver(SimpleKickerObserver<Input,State,Move,CFtype>& ob) { observer = &ob; }
  CFtype SelectKick(const State& st);
  CFtype BestKick(const State &st);
  CFtype FirstImprovingKick(const State &st);
  CFtype DenseBestKick(const State &st);
  CFtype RandomKick(const State &st);
  CFtype TotalFirstImprovingKick(const State &st) { throw std::logic_error("No TOTAL_FIRST_IMPROVING_KICK allowed for Simple Kickers"); }
  CFtype TotalBestKick(const State &st) { throw std::logic_error("No TOTAL_BEST_KICK allowed for Simple Kickers"); }
  bool SingleKicker() { return true; }

  void PrintCurrentMoves(unsigned int i, std::ostream& os) const  { os << current_moves[i]; }

  void MakeKick(State &st);
  virtual CFtype KickCost();
  virtual void PrintKick(std::ostream& os = std::cout) const;
  void SetStep(unsigned int s);
protected:
  void FirstKick(const State &st);
  bool NextKick();
  Move GetKickComponent(unsigned int i) const;
  void SetKickComponent(unsigned int i, const Move& mv);
  virtual bool RelatedMoves(const Move&, const Move&) const = 0;
  NeighborhoodExplorer<Input,State,Move,CFtype>& ne;
  std::vector<Move> current_moves, internal_best_moves; 
  CFtype current_kick_cost, best_kick_cost;
  bool FirstKickComponent(unsigned int i);  // used by the backtracking algorithm of bestkick
  bool NextKickComponent(unsigned int i);   // (idem)
  bool UnrelatedMoves(int i) const;         // (idem)

  SimpleKickerObserver<Input,State,Move,CFtype>* observer;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class State, class Move, typename CFtype>
SimpleKicker<Input,State,Move,CFtype>::SimpleKicker(const Input& in,
						    NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
						    unsigned int s, std::string name)
  : Kicker<Input,State,CFtype>(in, s, name), ne(e_ne), current_moves(s), internal_best_moves(s)
{  
  observer = nullptr;
}

template <class Input, class State, class Move, typename CFtype>
CFtype SimpleKicker<Input,State,Move,CFtype>::SelectKick(const State& st)
{
  if (observer != nullptr)
    observer->NotifyStartKicking(*this);
  switch (this->current_kick_type)
    {
    case RANDOM_KICK:
      return RandomKick(st);
    case BEST_KICK:
      return BestKick(st);
    case FIRST_IMPROVING_KICK:
      return FirstImprovingKick(st);
    case TOTAL_BEST_KICK:
      throw std::logic_error("No TOTAL_BEST_KICK allowed for Simple Kickers");
      break;
    case TOTAL_FIRST_IMPROVING_KICK:
      throw std::logic_error("No TOTAL_FIRST_IMPROVING_KICK allowed for Simple Kickers");
      break;
    default:
      throw std::logic_error("Unknown Kick Type for Simple Kickers");    
    }
  return 0;  // Only to prevent warnings (never reached)
}

template <class Input, class State, class Move, typename CFtype>
void SimpleKicker<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
  os  << "Simple Kicker: " << this->name << std::endl;
	
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
void SimpleKicker<Input,State,Move,CFtype>::SetStep(unsigned int s)
{
  Kicker<Input,State,CFtype>::SetStep(s);
  current_moves.resize(s);
  internal_best_moves.resize(s);
}

template <class Input, class State, class Move, typename CFtype>
bool SimpleKicker<Input,State,Move,CFtype>::FirstKickComponent(unsigned int i)
{
  if (i == 0)
    {
      ne.FirstMove(this->states[i], current_moves[i]);
      return true;
    }
  else
    return ne.FirstRelatedMove(this->states[i], current_moves[i], current_moves[i-1]);
}

template <class Input, class State, class Move, typename CFtype>
bool SimpleKicker<Input,State,Move,CFtype>::NextKickComponent(unsigned int i)
{
  if (i == 0)
    return ne.NextMove(this->states[i], current_moves[i]);
  else
    return ne.NextRelatedMove(this->states[i], current_moves[i], current_moves[i-1]);
}

template <class Input, class State, class Move, typename CFtype>
bool SimpleKicker<Input,State,Move,CFtype>::UnrelatedMoves(int i) const
{
  if (i <= 0)
    return false;
  else
    return !RelatedMoves(current_moves[i-1], current_moves[i]);
}

template <class Input, class State, class Move, typename CFtype>
CFtype SimpleKicker<Input,State,Move,CFtype>::DenseBestKick(const State &st)
{
  unsigned int dense_best_step = 0, max_step = this->step;
  CFtype dense_best_kick_cost = 0;
  std::vector<Move> dense_best_moves;
	
  for (this->step = 1; this->step <= max_step; this->step++)
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
  if (observer != nullptr)
    observer->NotifyBestKick(*this);
  while (NextKick())
    {
      if (observer != nullptr)
	observer->NotifyNewKick(*this);
      if (LessThan(current_kick_cost,best_kick_cost))
	{
	  best_kick_cost = current_kick_cost;
	  internal_best_moves = current_moves;
	  if (observer != nullptr)
	    observer->NotifyBestKick(*this);
	}
    }
  current_kick_cost = best_kick_cost;
  current_moves = internal_best_moves;
  if (observer != nullptr)
    observer->NotifyStopKicking(*this);
  return current_kick_cost;
}

template <class Input, class State, class Move, typename CFtype>
CFtype SimpleKicker<Input,State,Move,CFtype>::FirstImprovingKick(const State &st)
{
  FirstKick(st);
  best_kick_cost = current_kick_cost;
  internal_best_moves = current_moves;
  if (observer != nullptr)
    observer->NotifyBestKick(*this);
  if (LessThan(current_kick_cost,(CFtype)0)) return current_kick_cost;
  while (NextKick())
    {
      if (observer != nullptr)
	observer->NotifyNewKick(*this);
      if (LessThan(current_kick_cost,best_kick_cost))
	{
	  best_kick_cost = current_kick_cost;
	  internal_best_moves = current_moves;
	  if (observer != nullptr)
	    observer->NotifyBestKick(*this);
	  if (LessThan(current_kick_cost,(CFtype)0)) return current_kick_cost;
	}
    }
  current_kick_cost = best_kick_cost;
  current_moves = internal_best_moves;
  if (observer != nullptr)
    observer->NotifyStopKicking(*this);
  return current_kick_cost;
}

template <class Input, class State, class Move, typename CFtype>
void SimpleKicker<Input,State,Move,CFtype>::FirstKick(const State &st)
{ 
  int i = 0;
  this->states[0] = st;
  bool backtrack = FirstKickComponent(0);
  do
    {
      if (i == int(this->step - 1) && !backtrack)
	{ // the first kick has been found
	  current_kick_cost = KickCost();
	  return;
	}
      if (backtrack)
	{
	  if (NextKickComponent(i))
	    backtrack = false;
	  else
	    {
	      i--; // backtrack remains true
	      continue;	      
	    }
	}
      else
	{
	  this->states[i+1] = this->states[i];
	  ne.MakeMove(this->states[i+1],current_moves[i]);
	  i++;
	  if (!FirstKickComponent(i))
	    {
	      backtrack = true;
	      i--;
	      continue;
	    }
	  // else proceed: backtrack remains false

	}
      backtrack = UnrelatedMoves(i);
    }
  while (i >= 0);
  throw std::logic_error("No kick build in SimpleKicker::FirstKick()");
}

template <class Input, class State, class Move, typename CFtype>
bool SimpleKicker<Input,State,Move,CFtype>::NextKick()
{
  int i = this->step - 1;
  bool backtrack = true;
  do
    {
      if (i == int(this->step - 1) && !backtrack)
	{
	  current_kick_cost = KickCost();
	  return true;
	}
      if (backtrack)
	{
	  if (NextKickComponent(i))
	    backtrack = false;
	  else
	    {
	      i--; // backtrack remains true
	      continue;	      
	    }
	}
      else
	{
	  this->states[i+1] = this->states[i];
	  ne.MakeMove(this->states[i+1],current_moves[i]);
	  i++;
	  if (!FirstKickComponent(i))
	    {
	      backtrack = true;
	      i--;
	      continue;
	    }
	  // else proceed: backtrack remains false
	}
      backtrack = UnrelatedMoves(i);
    }
  while (i >= 0);
  return false;
}

template <class Input, class State, class Move, typename CFtype>
Move SimpleKicker<Input,State,Move,CFtype>::GetKickComponent(unsigned int i) const
{
  return current_moves[i];
}

template <class Input, class State, class Move, typename CFtype>
void SimpleKicker<Input,State,Move,CFtype>::SetKickComponent(unsigned int i, const Move& mv)
{
  current_moves[i] = mv;
}

template <class Input, class State, class Move, typename CFtype>
void SimpleKicker<Input,State,Move,CFtype>::MakeKick(State &st)
{
  for (unsigned int i = 0; i < this->step; i++)
    ne.MakeMove(st,current_moves[i]);
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

template <class Input, class State, class Move, typename CFtype>
void SimpleKicker<Input,State,Move,CFtype>::PrintKick(std::ostream& os) const
{
  for (unsigned int i = 0; i < this->step; i++)
    os << current_moves[i] << '[' << ne.DeltaCostFunction(this->states[i],current_moves[i]) << "] ";		
  os << std::endl;
}

#endif // _SIMPLE_KICKER_HH_