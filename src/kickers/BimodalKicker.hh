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

#if !defined(_BIMODAL_KICKER_HH_)
#define _BIMODAL_KICKER_HH_

#include <observers/BimodalKickerObserver.hh>
#include <kickers/Kicker.hh>

#if !defined(MOVE_ENUM)
typedef enum {
  MOVE_1 = 1,
  MOVE_2
} PatternMove;
#define MOVE_ENUM
#endif



typedef std::vector<PatternMove> PatternType;

/** The Bimodal Kicker compounds two different kind of moves given by
    template instantiation.
    @ingroup Perturbers 
*/
template <class Input, class State, class Move1, class Move2, typename CFtype = int>
class BimodalKicker
  : public Kicker<Input,State,CFtype>
{
  friend class BimodalKickerObserver<Input,State,Move1,Move2,CFtype>;
public:
  BimodalKicker(const Input& in, 
		NeighborhoodExplorer<Input,State,Move1,CFtype>& nhe1,
		NeighborhoodExplorer<Input,State,Move2,CFtype>& nhe2,
		unsigned int step,
		std::string name);
  void Print(std::ostream& os = std::cout) const;
  void MakeKick(State &st);
  void AttachObserver(BimodalKickerObserver<Input,State,Move1,Move2,CFtype>& ob) { observer = &ob; }
  Move1 CurrentMoves1(unsigned int i) const { return current_moves1[i]; }
  Move2 CurrentMoves2(unsigned int i) const { return current_moves2[i]; }
  void SetPattern(PatternType p) { pattern = p; }
  PatternType GetPattern() { return pattern; }
  void PrintPattern(std::ostream& os = std::cout);
  virtual void SetStep(unsigned int s);
  virtual void PrintKick(std::ostream& os = std::cout) const;
	
  CFtype SelectKick(const State& st);
  CFtype BestKick(const State &st);
  CFtype FirstImprovingKick(const State &st);
  CFtype DenseBestKick(const State &st) { throw std::runtime_error("Not implemented yet!"); }
  CFtype TotalFirstImprovingKick(const State &st);
  CFtype TotalBestKick(const State &st);
  CFtype RandomKick(const State &st);
  bool SingleKicker() { return false; }

  void PrintCurrentMoves(unsigned i, std::ostream& os) const  { if (pattern[i] == MOVE_1) os << current_moves1[i]; else os << current_moves2[i];}

  virtual bool RelatedMoves(const Move1 &mv1, const Move1 &mv2) const = 0;
  virtual bool RelatedMoves(const Move1 &mv1, const Move2 &mv2) const = 0;
  virtual bool RelatedMoves(const Move2 &mv1, const Move1 &mv2) const = 0;
  virtual bool RelatedMoves(const Move2 &mv1, const Move2 &mv2) const = 0;
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
protected:
  NeighborhoodExplorer<Input,State,Move1,CFtype>& nhe1;
  NeighborhoodExplorer<Input,State,Move2,CFtype>& nhe2;
  std::vector<Move1> current_moves1, internal_best_moves1; 
  std::vector<Move2> current_moves2, internal_best_moves2;
  PatternType pattern;
  PatternType best_pattern;
  CFtype current_kick_cost, best_kick_cost;
  CFtype KickCost();
  void FirstKickComponent(unsigned int i);  // used by the backtracking algorithm of bestkick
  bool NextKickComponent(unsigned int i);   // (idem)
  bool UnrelatedMoves(unsigned int i);      // (idem)
  void FirstKick(const State &st);
  bool NextKick();
private:
  void FirstPattern();
  bool NextPattern();
  BimodalKickerObserver<Input,State,Move1,Move2,CFtype>* observer;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class State, class Move1, class Move2, typename CFtype>
BimodalKicker<Input,State,Move1,Move2,CFtype>::BimodalKicker(const Input& i,
							     NeighborhoodExplorer<Input,State,Move1,CFtype>& nhe1,
							     NeighborhoodExplorer<Input,State,Move2,CFtype>& nhe2,
							     unsigned int s,
							     std::string name)
  : Kicker<Input,State,CFtype>(i,s,name), nhe1(nhe1), nhe2(nhe2),
    current_moves1(s), internal_best_moves1(s), 
  current_moves2(s), internal_best_moves2(s), pattern(s) 
{
  for (unsigned int i = 0; i < s; i++)
    { // Default pattern is 1 2 1 2 ...
      if ((i % 2) == 0)
	pattern[i] = MOVE_1;
      else
	pattern[i] = MOVE_2;
    }  
  observer = NULL;
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
CFtype BimodalKicker<Input,State,Move1,Move2,CFtype>::SelectKick(const State& st)
{
  if (observer != NULL)
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
      return TotalBestKick(st);
    case TOTAL_FIRST_IMPROVING_KICK:
      return TotalFirstImprovingKick(st);
    default:
      throw std::logic_error("Unknown Kick Type for Bimodal Kickers");    
    }
  return 0;  // Only to prevent warnings (never reached)
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalKicker<Input,State,Move1,Move2,CFtype>::Print(std::ostream& os) const
{
  os  << "Bimodal Kicker: " << this->name << std::endl;
	
  os  << "Step: " << this->step << std::endl;
  os  << "Kick selection: ";
  switch (this->current_kick_type)
    {
    case RANDOM_KICK:
      os << "RANDOM" << std::endl;
      os  << "Pattern: ";
      for (unsigned int i = 0; i < pattern.size(); i++)
	os << pattern[i] << " ";
      os << std::endl;
      break;
    case BEST_KICK:
      os << "BEST" << std::endl;
      os  << "Pattern: ";
      for (unsigned int i = 0; i < pattern.size(); i++)
	os << pattern[i] << " ";
      os << std::endl;
      break;
    case TOTAL_BEST_KICK:
      os << "TOTAL BEST" << std::endl;
      break;
    case FIRST_IMPROVING_KICK:
      os << "FIRST_IMPROVING" << std::endl;
      os  << "Pattern: ";
      for (unsigned int i = 0; i < pattern.size(); i++)
	os << pattern[i] << " ";
      os << std::endl;
      break;
    case TOTAL_FIRST_IMPROVING_KICK:
      os << "TOTAL FIRST_IMPROVING" << std::endl;
      break;
    }
	
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalKicker<Input,State,Move1,Move2,CFtype>::SetStep(unsigned int s)
{
  Kicker<Input,State,CFtype>::SetStep(s);
  current_moves1.resize(s);
  internal_best_moves1.resize(s);
  current_moves2.resize(s);
  internal_best_moves2.resize(s);
  pattern.resize(s);
  best_pattern.resize(s);
  for (unsigned int i = 0; i < s; i++)
    {// Default pattern is 1 2 1 2 ...
      if ((i % 2) == 0)
	pattern[i] = MOVE_1;
      else 
	pattern[i] = MOVE_2;
    }
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalKicker<Input,State,Move1,Move2,CFtype>::FirstKickComponent(unsigned int i)
{
  if (pattern[i] == MOVE_1)
    {
      nhe1.FirstMove(this->states[i], current_moves1[i]);
    }
  else
    {
      nhe2.FirstMove(this->states[i], current_moves2[i]);
    }
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
bool BimodalKicker<Input,State,Move1,Move2,CFtype>::NextKickComponent(unsigned int i)
{
  if (pattern[i] == MOVE_1)
    {
      return nhe1.NextMove(this->states[i], current_moves1[i]);
    }
  else
    {
      return nhe2.NextMove(this->states[i], current_moves2[i]);
    }
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
bool BimodalKicker<Input,State,Move1,Move2,CFtype>::UnrelatedMoves(unsigned int i)
{
  if (i == 0)
    return false;
  else
    if (pattern[i-1] == MOVE_1 && pattern[i] == MOVE_1)
      return !RelatedMoves(current_moves1[i-1], current_moves1[i]);
    else if (pattern[i-1] == MOVE_1 && pattern[i] == MOVE_2)
      return !RelatedMoves(current_moves1[i-1], current_moves2[i]);
    else if (pattern[i-1] == MOVE_2 && pattern[i] == MOVE_1)
      return !RelatedMoves(current_moves2[i-1], current_moves1[i]);
    else // if (pattern[i-1] == MOVE_2 && pattern[i] == MOVE_2)
      return !RelatedMoves(current_moves2[i-1], current_moves2[i]);
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
CFtype BimodalKicker<Input,State,Move1,Move2,CFtype>::BestKick(const State &st)
{
  bool first_kick_found = false;
  int i = 0;
  this->states[0] = st;
  FirstKickComponent(0);
  do
    {
      bool backtrack = UnrelatedMoves(i);
      if (i == int(this->step - 1) && !backtrack)
	{
	  if (observer != NULL)
	    observer->NotifyNewKick(*this);
	  if (!first_kick_found)
	    {
	      current_kick_cost = KickCost();
	      best_kick_cost = current_kick_cost;
	      internal_best_moves1 = current_moves1;
	      internal_best_moves2 = current_moves2;
	      first_kick_found = true;
	      if (observer != NULL)
		observer->NotifyBestKick(*this);
	    }
	  else
	    {
	      current_kick_cost = KickCost();
	      if (LessThan(current_kick_cost,best_kick_cost))
		{
		  best_kick_cost = current_kick_cost;
		  internal_best_moves1 = current_moves1;
		  internal_best_moves2 = current_moves2;
		  if (observer != NULL)
		    observer->NotifyBestKick(*this);
		}
	    }
	  backtrack = true;
	}
      if (backtrack)
	do
	  {
	    if (NextKickComponent(i))
	      backtrack = false;
	    else
	      i--;
	  }
	while (backtrack && i >= 0);
      else
	{
	  this->states[i+1] = this->states[i];
	  if (pattern[i] == MOVE_1)
	    nhe1.MakeMove(this->states[i+1],current_moves1[i]);
	  else
	    nhe2.MakeMove(this->states[i+1],current_moves2[i]);
	  i++;
	  FirstKickComponent(i);
	}
    }
  while (i >= 0);
  
  current_kick_cost = best_kick_cost;
  current_moves1 = internal_best_moves1;
  current_moves2 = internal_best_moves2;
  if (observer != NULL)
    observer->NotifyStopKicking(*this);
  return current_kick_cost;
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
CFtype BimodalKicker<Input,State,Move1,Move2,CFtype>::FirstImprovingKick(const State &st)
{
  bool first_kick_found = false;
  int i = 0;
  this->states[0] = st;
  FirstKickComponent(0);
  do
    {
      bool backtrack = UnrelatedMoves(i);
      if (i == int(this->step - 1) && !backtrack)
	{
	  if (observer != NULL)
	    observer->NotifyNewKick(*this);
	  if (!first_kick_found)
	    {
	      current_kick_cost = KickCost();
	      best_kick_cost = current_kick_cost;
	      internal_best_moves1 = current_moves1;
	      internal_best_moves2 = current_moves2;
	      first_kick_found = true;
	      if (observer != NULL)
		observer->NotifyBestKick(*this);
	      if (LessThan(current_kick_cost,0))
		return current_kick_cost;
	    }
	  else
	    {
	      current_kick_cost = KickCost();
	      if (LessThan(current_kick_cost,best_kick_cost))
		{
		  best_kick_cost = current_kick_cost;
		  internal_best_moves1 = current_moves1;
		  internal_best_moves2 = current_moves2;
		  if (observer != NULL)
		    observer->NotifyBestKick(*this);
		  if (LessThan(current_kick_cost,0))
		    return current_kick_cost;
		}
	    }
	  backtrack = true;
	}
      if (backtrack)
	do
	  {
	    if (NextKickComponent(i))
	      backtrack = false;
	    else
	      i--;
	  }
	while (backtrack && i >= 0);
      else
	{
	  this->states[i+1] = this->states[i];
	  if (pattern[i] == MOVE_1)
	    nhe1.MakeMove(this->states[i+1],current_moves1[i]);
	  else
	    nhe2.MakeMove(this->states[i+1],current_moves2[i]);
	  i++;
	  FirstKickComponent(i);
	}
    }
  while (i >= 0);

  current_kick_cost = best_kick_cost;
  current_moves1 = internal_best_moves1;
  current_moves2 = internal_best_moves2;
  if (observer != NULL)
    observer->NotifyStopKicking(*this);
  return current_kick_cost;
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalKicker<Input,State,Move1,Move2,CFtype>::FirstKick(const State &st)
{
  int i = 0;
  this->states[0] = st;
  FirstKickComponent(0);
  do
    {
      bool backtrack = UnrelatedMoves(i);
      if (i == int(this->step - 1) && !backtrack)
	{
	  current_kick_cost = KickCost();
	  best_kick_cost = current_kick_cost;
	  internal_best_moves1 = current_moves1;
	  internal_best_moves2 = current_moves2;
	  return;
	}
      if (backtrack)
	do
	  {
	    if (NextKickComponent(i))
	      backtrack = false;
	    else
	      i--;
	  }
	while (backtrack && i >= 0);
      else
	{
	  this->states[i+1] = this->states[i];
	  if (pattern[i] == MOVE_1)
	    nhe1.MakeMove(this->states[i+1],current_moves1[i]);
	  else
	    nhe2.MakeMove(this->states[i+1],current_moves2[i]);
	  i++;
	  FirstKickComponent(i);
	}
    }
  while (i >= 0);
  throw std::logic_error("No kick build in BimodalKicker::FirstKick()");
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
bool BimodalKicker<Input,State,Move1,Move2,CFtype>::NextKick()
{
  int i = this->step - 1;
  bool backtrack = true;
  do
    {
      if (i == int(this->step - 1) && !backtrack)
	{
	  current_kick_cost = KickCost();
	  best_kick_cost = current_kick_cost;
	  internal_best_moves1 = current_moves1;
	  internal_best_moves2 = current_moves2;
	  return true;
	}
      if (backtrack)
	do
	  {
	    if (NextKickComponent(i))
	      backtrack = false;
	    else
	      i--;
	  }
	while (backtrack && i >= 0);
      else
	{
	  this->states[i+1] = this->states[i];
	  if (pattern[i] == MOVE_1)
	    nhe1.MakeMove(this->states[i+1],current_moves1[i]);
	  else
	    nhe2.MakeMove(this->states[i+1],current_moves2[i]);
	  i++;
	  FirstKickComponent(i);
	}
      backtrack = UnrelatedMoves(i);
    }
  while (i >= 0);
  return false;
}


template <class Input, class State, class Move1, class Move2, typename CFtype>
CFtype BimodalKicker<Input,State,Move1,Move2,CFtype>::TotalBestKick(const State &st)
{
  // NOTE: assumes one consistent kick for any pattern (it would need to capture the exceptions)
  CFtype total_best_cost;
  FirstPattern();
  best_kick_cost = BestKick(st);
  total_best_cost = best_kick_cost;
  std::vector<Move1> total_best_moves1 = internal_best_moves1;
  std::vector<Move2> total_best_moves2 = internal_best_moves2;
  while(NextPattern())
    {
      best_kick_cost = BestKick(st);
      if (LessThan(best_kick_cost, total_best_cost))
	{
	  total_best_cost = best_kick_cost;
	  total_best_moves1 = internal_best_moves1;
	  total_best_moves2 = internal_best_moves2;
	  best_pattern = pattern;
	}
    }
  current_moves1 = total_best_moves1;
  current_moves2 = total_best_moves2;
  pattern = best_pattern;

  return best_kick_cost;
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
CFtype BimodalKicker<Input,State,Move1,Move2,CFtype>::TotalFirstImprovingKick(const State &st)
{
  FirstPattern();
  CFtype first_kick_cost = FirstImprovingKick(st);
  if (LessThan(current_kick_cost, 0)) return first_kick_cost;
  
  while(NextPattern())
    {
      first_kick_cost = FirstImprovingKick(st);
      if (LessThan(current_kick_cost, 0)) return first_kick_cost;
    }
  return first_kick_cost;  // if no improving found, return the last
}


template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalKicker<Input,State,Move1,Move2,CFtype>::MakeKick(State &st)
{
  for (unsigned int i = 0; i < this->step; i++)
    {
      if (pattern[i] == MOVE_1)
	{
	  nhe1.MakeMove(st,current_moves1[i]);
	}
      else
	{
	  nhe2.MakeMove(st,current_moves2[i]);
	}
    }
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
CFtype BimodalKicker<Input,State,Move1,Move2,CFtype>::RandomKick(const State &st)
{
  this->states[0] = st;
  for (unsigned int i = 0; i < this->step; i++)
    {
      if (pattern[i] == MOVE_1)
	{
	  nhe1.RandomMove(this->states[i],current_moves1[i]);
	  this->states[i+1] = this->states[i];
	  nhe1.MakeMove(this->states[i+1],current_moves1[i]);
	}
      else
	{
	  nhe2.RandomMove(this->states[i],current_moves2[i]);
	  this->states[i+1] = this->states[i];
	  nhe2.MakeMove(this->states[i+1],current_moves2[i]);
	}
    }
  return KickCost();
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
CFtype BimodalKicker<Input,State,Move1,Move2,CFtype>::KickCost()
{
  CFtype cost = 0;
  for (unsigned int i = 0; i < this->step; i++)
    if (pattern[i] == MOVE_1)
      {
	cost += nhe1.DeltaCostFunction(this->states[i], current_moves1[i]);
      }
    else
      {
	cost += nhe2.DeltaCostFunction(this->states[i], current_moves2[i]);
      }
  return cost;
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalKicker<Input,State,Move1,Move2,CFtype>::FirstPattern()
{
  for (unsigned int i = 0; i < pattern.size(); i++)
    pattern[i] = MOVE_1;
  best_pattern = pattern;
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
bool BimodalKicker<Input,State,Move1,Move2,CFtype>::NextPattern()
{
  for (unsigned int i = 0; i < pattern.size(); i++)
    if (pattern[i] == MOVE_1)
      {
	pattern[i] = MOVE_2;
	return true;
      }
    else
      pattern[i] = MOVE_1;
  return false;
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalKicker<Input,State,Move1,Move2,CFtype>::ReadParameters(std::istream& is , std::ostream& os)
{
  unsigned s;
  os << "BIMODAL KICKER -- INPUT PARAMETERS" << std::endl;
  os << "  Step: ";
  is >> s;
  SetStep(s);
  os << "  Pattern: ";
  for (unsigned int i = 0; i < this->step; i++)
    {
      unsigned int tmp;
      is >> tmp;
      if (tmp == 1)
	pattern[i] = MOVE_1;
      else if (tmp == 2)
	pattern[i] = MOVE_2;
      else
	os << "Wrong move type while pattern input" << std::endl;
    }
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalKicker<Input,State,Move1,Move2,CFtype>::PrintKick(std::ostream& os) const
{
  for (unsigned int i = 0; i < this->step; i++)
    if (pattern[i] == MOVE_1)
      os << current_moves1[i] << '[' << nhe1.DeltaCostFunction(this->states[i],current_moves1[i]) << "] ";
    else
      os << current_moves2[i] << '[' << nhe2.DeltaCostFunction(this->states[i],current_moves2[i]) << "] ";
  os << std::endl;
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalKicker<Input,State,Move1,Move2,CFtype>::PrintPattern(std::ostream& os)
{
  unsigned int i;
  for (i = 0; i < this->step - 1; i++)
    os << pattern[i] << ' ';
  os << pattern[i];
}
#endif // _BIMODAL_KICKER_HH_
