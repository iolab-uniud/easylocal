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

#if !defined(_MULTIMODALNEIGHBORHOOD_EXPLORER_HH_)
#define _MULTIMODALNEIGHBORHOOD_EXPLORER_HH_

#include <helpers/DeltaCostComponent.hh>
#include <helpers/StateManager.hh>
#include <helpers/ProhibitionManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <stdexcept>

template <typename T1, typename T2>
class inspect_types 
{
public:
  static bool are_equal() 
  { return false; }
};

template <typename T>
class inspect_types<T, T>
{
public:
  static bool are_equal() 
  { return true; }
};

class NullType 
{
public:
  enum { length = 0 };
};

template <typename H, typename T>
class Typelist
{
public:
  typedef H Head;
  typedef T Tail;  
  enum { length = Tail::length + 1 };
};

#define TYPELIST_1(Type) Typelist<Type, NullType>
#define TYPELIST_2(Type1, Type2) Typelist<Type1, TYPELIST_1(Type2) >
#define TYPELIST_3(Type1, Type2, Type3) Typelist<Type1, TYPELIST_2(Type2, Type3) >
#define TYPELIST_4(Type1, Type2, Type3, Type4) Typelist<Type1, TYPELIST_3(Type2, Type3, Type4) >

template <typename CFtype, typename H, typename T>
class Movelist
{
public:
  typedef H Head;
  typedef T Tail;
  H move;
  T movelist;
    
  bool selected;
  Movelist() : selected(false) {}
  Movelist(const Movelist<CFtype, H, T>& ml) : move(ml.move), movelist(ml.movelist), selected(ml.selected) {}
  
  enum { length = Tail::length + 1 };
};

template <typename CFtype, typename H>
class Movelist<CFtype, H, NullType>
{
public:
  typedef H Head;
  typedef NullType Tail;
  H move;
  NullType movelist;
  
  bool selected;
  
  Movelist() : selected(false) {}
  Movelist(const Movelist<CFtype, H, NullType>& ml) : move(ml.move), selected(ml.selected) {}
  enum { length = 1 };
};

// FIXME: this is not the right way to read and write this kind of move

template <typename CFtype, typename H, typename T>
std::ostream& operator<<(std::ostream& os, const Movelist<CFtype, H, T>& mv)
{
  os << mv.move << "[ " << mv.selected << " ]" << mv.movelist;
  
  return os;
}

template <typename CFtype, typename H>
std::ostream& operator<<(std::ostream& os, const Movelist<CFtype, H, NullType>& mv)
{
  os << mv.move << "[ " << mv.selected << " ]";
  
  return os;
}

template <typename CFtype, typename H, typename T>
std::istream& operator>>(std::istream& is, Movelist<CFtype, H, T>& mv)
{
  char c;
  is >> mv.move >> c >> mv.selected >> c >> mv.movelist;
  
  return is;
}

template <typename CFtype, typename H>
std::istream& operator>>(std::istream& is, Movelist<CFtype, H, NullType>& mv)
{
  is >> mv.move;
  
  return is;
}

template <typename CFtype, typename H, typename T>
bool operator==(const Movelist<CFtype, H, T>& mv1, const Movelist<CFtype, H, T>& mv2)
{
  if (mv1.selected != mv2.selected)
    return false;
  else if (mv1.selected && mv2.selected)
    return mv1.move == mv2.move && mv1.movelist == mv2.movelist;
  else
    return mv1.movelist == mv2.movelist;
}

template <typename CFtype, typename H>
bool operator==(const Movelist<CFtype, H, NullType>& mv1, const Movelist<CFtype, H, NullType>& mv2)
{
  if (mv1.selected != mv2.selected)
    return false;
  else if (mv1.selected && mv2.selected)
    return mv1.move == mv2.move;
  else
    return true;
}

template <typename CFtype, typename H, typename T>
bool operator<(const Movelist<CFtype, H, T>& mv1, const Movelist<CFtype, H, T>& mv2)
{
  if (mv1.selected && mv2.selected)
    {
      if (mv1.move < mv2.move)
	return true;
      else if (mv2.move < mv1.move)
	return false;
      else // mv1.move == mv2.move
	return mv1.movelist < mv2.movelist;
    }
  else if (mv1.selected && !mv2.selected)
    return true;
  else if (!mv1.selected && mv2.selected)
    return false;
  else
    return mv1.movelist < mv2.movelist;
}

template <typename CFtype, typename H>
bool operator<(const Movelist<CFtype, H, NullType>& mv1, const Movelist<CFtype, H, NullType>& mv2)
{
  if (mv1.selected && mv2.selected)
    {
      return mv1.move < mv2.move;
    }
  else 
    {      
      if (mv1.selected && !mv2.selected)
	return true;
      else if (!mv1.selected && mv2.selected)
	return false;
      else
	return false;
    }
}

/** The SetUnion Neighborhood Explorer is responsible for the strategy
 exploited in the exploration of the combination of the neighborhoods obtained
 by means of the set-union operator.
 It can be used directly in a Runner through an adapter, which adjust
 a SetUnion Neighborhood Explorer in order to fit it within the usual 
 Runner hierarchy.
 
 @ingroup Helpers
 */

template <class Input, class State, typename CFtype, class NeighborhoodExplorerList>
class SetUnionNeighborhoodExplorer
{
protected:
  typedef typename NeighborhoodExplorerList::Head ThisNeighborhoodExplorer;
  typedef SetUnionNeighborhoodExplorer<Input, State, CFtype, typename NeighborhoodExplorerList::Tail> OtherNeighborhoodExplorers;
public:   
  typedef Movelist<CFtype, typename ThisNeighborhoodExplorer::ThisMove, typename OtherNeighborhoodExplorers::MoveList> MoveList;
  
  /**
   Adds a (monomodal) neighborhood explorer to the pool.
   @param ne the neighborhood explorer to be added
   */
  template <typename NeighborhoodExplorer>
  void AddNeighborhoodExplorer(NeighborhoodExplorer& ne)
  {
    if (inspect_types<NeighborhoodExplorer, ThisNeighborhoodExplorer>::are_equal() && p_nhe == NULL) // the second condition is just to allow duplicated types in the typelist
      p_nhe = dynamic_cast<ThisNeighborhoodExplorer*>(&ne); // only to prevent a compilation error
    else
      other_nhes->AddNeighborhoodExplorer(ne);
  }
    
  /** 
   Generates a random move in the neighborhood of a given state.	

   @param st the start state 
   @param mv the generated move 
   */
  void RandomMove(const State &st, MoveList& mv) const;
  /**
   Generates the first move in the exploration of the neighborhood.

   @param st the start state
   @param mv the generated move
   */
  void FirstMove(const State& st, MoveList& mv) const;
  /** Generates the move that follows mv in the exploration of the
   neighborhood of the state st. 
   It returns the generated move in the same variable mv.
   Returns false if mv is the last in the state.
   
   @param st the start state 
   @param mv the move 
   */
  bool NextMove(const State &st, MoveList& mv) const;
  
  /** Performs the move mv on the state st. 
     
   @param st the start state 
   @param mv the move 
   */
  void MakeMove(State& st, const MoveList& mv) const;
  
  
  /** Evaluates the variation of the cost function obtained by applying the given
   move to the state.
   
   @param st the start state 
   @param mv the move 
   */
  CFtype DeltaCostFunction(const State& st, const MoveList& mv) const;
  
  /** Evaluates the variation of the cost function obtained by applying the given
   move to the state.
   
   @param st the start state 
   @param mv the move 
   */
  ShiftedResult<CFtype> DeltaShiftedCostFunction(const State& st, const MoveList& mv) const;
  
  /**
   Constructs a set-union multimodal neighborhood explorer passing a reference to a state manager 
   and to the input.
   
   @param in a pointer to an input object.
   @param sm a pointer to a compatible state manager.
   @param name the name associated to the NeighborhoodExplorer.
   */
  SetUnionNeighborhoodExplorer(const Input& in, StateManager<Input,State,CFtype>& sm, std::string name = "SetUnionNeighborhoodExplorer");
  
  /**
   Constructs a set-union multimodal neighborhood explorer passing a reference to a state manager, a bias vector for the random picking probability 
   and a reference to the input.
   
   @param in a pointer to an input object.
   @param sm a pointer to a compatible state manager.
   @param bias a sum 1 vector with a bias for the random picking probability
   @param name the name associated to the NeighborhoodExplorer.
   */
  SetUnionNeighborhoodExplorer(const Input& in, StateManager<Input,State,CFtype>& sm, const std::vector<double>& bias, std::string name = "SetUnionNeighborhoodExplorer");
  
  /** Destructs a set-union multimodal neighborhood explorer */
  ~SetUnionNeighborhoodExplorer();
  
  /** 
   Sets all moves in a movelist to unselected.
   */
  void SetAllUnselected(MoveList& mv) const;
  
  
  /** 
   Generates a random move in the neighborhood of a given state according to the move bias.	
   
   @param st the start state 
   @param mv the generated move 
   @param random_value a uniformly distributed random value to be compared to the bias
   */
  void RandomMove(const State &st, MoveList& mv, double random_value) const;
  
  /**
   Constructs a set-union multimodal neighborhood explorer passing a reference to a state manager, the range of the (possibly) biased probability distribution
   and a reference to the input.
   
   @param in a pointer to an input object.
   @param sm a pointer to a compatible state manager.
   @param bias a sum 1 vector with a bias for the random picking probability
   @param index the index up to which summing the bias
   @param name the name associated to the NeighborhoodExplorer.
   */
  SetUnionNeighborhoodExplorer(const Input& in, StateManager<Input,State,CFtype>& sm, const std::vector<double>& bias, unsigned int index, std::string name = "SetUnionNeighborhoodExplorer");

  unsigned int Modality() const;
  unsigned int MoveModality(const MoveList& mv) const;
  
protected:
  const Input& in;/**< A reference to the input object */
  StateManager<Input, State, CFtype>& sm; /**< A reference to the attached state manager. */
     
  ThisNeighborhoodExplorer* p_nhe;
  OtherNeighborhoodExplorers* other_nhes;
  std::string name;
  double probability_l, probability_u;
};


/** The CartesianProduct Neighborhood Explorer is responsible for the strategy
 exploited in the exploration of the combination of the neighborhoods obtained
 by means of the cartesian product operator.
 It can be used directly in a Runner through an adapter, which adjust
 a SetUnion Neighborhood Explorer in order to fit it within the usual 
 Runner hierarchy.
 
 @ingroup Helpers
 */

template <class Input, class State, typename CFtype, class NeighborhoodExplorerList>
class CartesianProductNeighborhoodExplorer
{
protected:
  typedef typename NeighborhoodExplorerList::Head ThisNeighborhoodExplorer;
  typedef CartesianProductNeighborhoodExplorer<Input, State, CFtype, class NeighborhoodExplorerList::Tail> OtherNeighborhoodExplorers;
  
public:   
  typedef Movelist<CFtype, typename ThisNeighborhoodExplorer::ThisMove, typename OtherNeighborhoodExplorers::MoveList> MoveList;
  
  /**
   Adds a (monomodal) neighborhood explorer to the pool.
   @param ne the neighborhood explorer to be added
   */
  template <typename NeighborhoodExplorer>
  void AddNeighborhoodExplorer(NeighborhoodExplorer& ne)
  {
    if (inspect_types<NeighborhoodExplorer, ThisNeighborhoodExplorer>::are_equal() && p_nhe == NULL) // the second condition is just to allow duplicated types in the typelist
      p_nhe = dynamic_cast<ThisNeighborhoodExplorer*>(&ne); // only to prevent a compilation error
    else
      other_nhes.AddNeighborhoodExplorer(ne);
  }
  
  /** 
   Generates a random move in the neighborhood of a given state.	
   
   @param st the start state 
   @param mv the generated move 
   */
  void RandomMove(const State &st, MoveList& mv) const;
  /**
   Generates the first move in the exploration of the neighborhood.
   
   @param st the start state
   @param mv the generated move
   */
  void FirstMove(const State& st, MoveList& mv) const;
  /** Generates the move that follows mv in the exploration of the
   neighborhood of the state st. 
   It returns the generated move in the same variable mv.
   Returns false if mv is the last in the state.
   
   @param st the start state 
   @param mv the move 
   */
  bool NextMove(const State &st, MoveList& mv) const;
  
  /** Performs the move mv on the state st. 
   
   @param st the start state 
   @param mv the move 
   */
  void MakeMove(State& st, const MoveList& mv) const;
  
  
  /** Evaluates the variation of the cost function obtained by applying the given
   move to the state.
   
   @param st the start state 
   @param mv the move 
   */
  CFtype DeltaCostFunction(const State& st, const MoveList& mv) const;
  
  /** Evaluates the variation of the cost function obtained by applying the given
   move to the state.
   
   @param st the start state 
   @param mv the move 
   */
  ShiftedResult<CFtype> DeltaShiftedCostFunction(const State& st, const MoveList& mv) const;
  
  /**
   Constructs a cartesian product multimodal neighborhood explorer passing a reference to a state manager 
   and to the input.
   
   @param in a pointer to an input object.
   @param sm a pointer to a compatible state manager.
   @param name the name associated to the NeighborhoodExplorer.
   */
  CartesianProductNeighborhoodExplorer(const Input& in, StateManager<Input,State,CFtype>& sm, std::string name = "CartesianProductNeighborhoodExplorer");
  
  CartesianProductNeighborhoodExplorer(const Input& in, StateManager<Input,State,CFtype>& sm, const std::vector<double>& bias, std::string name = "CartesianProductNeighborhoodExplorer");
  
  unsigned int Modality() const;
  unsigned int MoveModality(const MoveList& mv) const;
protected:
  const Input& in;/**< A reference to the input object */
  StateManager<Input, State, CFtype>& sm; /**< A reference to the attached state manager. */
  
  ThisNeighborhoodExplorer* p_nhe;
  OtherNeighborhoodExplorers other_nhes;
  std::string name;
};


/** This class adapts the MultimodalNeighborhoodExplorer interface to the NeighborhoodExplorer type 
 to allow using it with runners and other EasyLocal components.
*/

template <typename Input, typename State, typename MoveList, typename MultimodalNeighborhoodExplorer, typename CFtype = int>
class MultimodalNeighborhoodExplorerAdapter : public NeighborhoodExplorer<Input, State, MoveList, CFtype>
{
public:
  MultimodalNeighborhoodExplorer mmnhe;
  
  MultimodalNeighborhoodExplorerAdapter(const Input& in, StateManager<Input,State,CFtype>& sm, std::string name = "MultimodalNeighborhoodExplorerAdapter")
    : NeighborhoodExplorer<Input, State, MoveList, CFtype>(in, sm, name), mmnhe(in, sm, name) {}

  MultimodalNeighborhoodExplorerAdapter(const Input& in, StateManager<Input,State,CFtype>& sm, const std::vector<double>& bias, std::string name = "MultimodalNeighborhoodExplorerAdapter")
  : NeighborhoodExplorer<Input, State, MoveList, CFtype>(in, sm, name), mmnhe(in, sm, bias, name) {}
    
  unsigned int Modality() const
  { return mmnhe.Modality(); }
  
  unsigned int MoveModality(const MoveList& mv) const
  { return mmnhe.MoveModality(mv); }
  
  template <typename NeighborhoodExplorer>
  void AddNeighborhoodExplorer(NeighborhoodExplorer& ne)
  {
    mmnhe.AddNeighborhoodExplorer(ne);
  }

  void RandomMove(const State &st, MoveList& mv) const
  {
    mmnhe.RandomMove(st, mv);
  }
  
  void FirstMove(const State& st, MoveList& mv) const
  {
    mmnhe.FirstMove(st, mv);
  }

  bool NextMove(const State &st, MoveList& mv) const
  {
    return mmnhe.NextMove(st, mv);
  }

  void MakeMove(State& st, const MoveList& mv) const
  {
    mmnhe.MakeMove(st, mv);
  }

  CFtype DeltaCostFunction(const State& st, const MoveList& mv) const
  {
    return mmnhe.DeltaCostFunction(st, mv);
  }
  
  ShiftedResult<CFtype> DeltaShiftedCostFunction(const State& st, const MoveList& mv) const
  {
    return mmnhe.DeltaShiftedCostFunction(st, mv);
  }
};

template <typename Input, typename State, typename NeighborhoodExplorerList, typename CFtype = int>
class PrepareSetUnionNeighborhoodExplorerTypes
{
public:
  typedef SetUnionNeighborhoodExplorer<Input, State, CFtype, NeighborhoodExplorerList> MultimodalNeighborhoodExplorerType;  
  typedef typename MultimodalNeighborhoodExplorerType::MoveList MoveList;
  typedef MultimodalNeighborhoodExplorerAdapter<Input, State, MoveList, MultimodalNeighborhoodExplorerType, CFtype> NeighborhoodExplorer;
};

template <typename Input, typename State, typename NeighborhoodExplorerList, typename CFtype = int>
class PrepareCartesianProductNeighborhoodExplorerTypes
{
public:
typedef CartesianProductNeighborhoodExplorer<Input, State, CFtype, NeighborhoodExplorerList> MultimodalNeighborhoodExplorerType;  
typedef typename MultimodalNeighborhoodExplorerType::MoveList MoveList;
typedef MultimodalNeighborhoodExplorerAdapter<Input, State, MoveList, MultimodalNeighborhoodExplorerType, CFtype> NeighborhoodExplorer;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

// SetUnion

template <class Input, class State, typename CFtype, class NeighborhoodExplorerList>
SetUnionNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::SetUnionNeighborhoodExplorer(const Input& i, 
                                                                                                        StateManager<Input,State,CFtype>& e_sm, 
                                                                                                        std::string e_name)
: in(i), sm(e_sm),  p_nhe(NULL), name(e_name)
{
  std::vector<double> bias(MoveList::length, 1.0 / MoveList::length);
  probability_l = 0;
  probability_u = bias[0];
  other_nhes = new SetUnionNeighborhoodExplorer<Input, State, CFtype, typename NeighborhoodExplorerList::Tail>(i, e_sm, bias, 1, e_name);
}

template <class Input, class State, typename CFtype, class NeighborhoodExplorerList>
SetUnionNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::SetUnionNeighborhoodExplorer(const Input& i, 
                                                                                                        StateManager<Input,State,CFtype>& e_sm, 
                                                                                                        const std::vector<double>& bias,
                                                                                                        std::string e_name)
: in(i), sm(e_sm),  p_nhe(NULL), name(e_name)
{
  probability_l = 0;
  probability_u = bias[0];
  other_nhes = new SetUnionNeighborhoodExplorer<Input, State, CFtype, typename NeighborhoodExplorerList::Tail>(i, e_sm, bias, 1, e_name);
}

template <class Input, class State, typename CFtype, class NeighborhoodExplorerList>
SetUnionNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::SetUnionNeighborhoodExplorer(const Input& i, 
                                                                                                        StateManager<Input,State,CFtype>& e_sm, 
                                                                                                        const std::vector<double>& bias,
                                                                                                        unsigned int index,
                                                                                                        std::string e_name)
: in(i), sm(e_sm),  p_nhe(NULL), name(e_name)
{
  probability_l = 0.0;
  for (unsigned int j = 0; j < index; j++)
    probability_l += bias[j];
  probability_u = probability_l + bias[index];
  other_nhes = new SetUnionNeighborhoodExplorer<Input, State, CFtype, typename NeighborhoodExplorerList::Tail>(i, e_sm, bias, index + 1, e_name);
}



template <class Input, class State, typename CFtype, class NeighborhoodExplorerList>
SetUnionNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::~SetUnionNeighborhoodExplorer()
{
  if (other_nhes)
    delete other_nhes;
}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
void SetUnionNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::RandomMove(const State &st, MoveList& mv) const
{
  /* mv.selected = false;
  if (mv.length == 1 || Random::Int(1, mv.length) == 1) {
    // with uniform probability select this move, or at last select this move
    try
    {
      p_nhe->RandomMove(st, mv.move);    
      mv.selected = true;
      other_nhes->SetAllUnselected(mv.movelist);
    }
    catch (EmptyNeighborhood e)
    {
      other_nhes->RandomMove(st, mv.movelist);
    }
  }
  else
    other_nhes->RandomMove(st, mv.movelist); */
  // new definition: first pick up a uniformly distributed random value and then search for the corresponding neighborhood according to the bias
  double random_value = Random::Double_Unit_Uniform();
  RandomMove(st, mv, random_value);
}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
void SetUnionNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::RandomMove(const State &st, MoveList& mv, double random_value) const
{
  mv.selected = false;
  if (mv.length == 1 || (random_value >= probability_l && random_value < probability_u)) {
    // with biased probability select this move, or at last select this move
    try
    {
      p_nhe->RandomMove(st, mv.move);    
      mv.selected = true;
      other_nhes->SetAllUnselected(mv.movelist);
    }
    catch (EmptyNeighborhood e)
    {
      random_value = Random::Double(random_value, 1.0); // go ahead with the following neighborhoods
      other_nhes->RandomMove(st, mv.movelist, random_value);
    }
  }
  else
    other_nhes->RandomMove(st, mv.movelist, random_value);
}



template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
void SetUnionNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::FirstMove(const State& st, MoveList& mv) const
{ 
  mv.selected = false; // to prevent inconsistent states
  if (mv.length == 1) // it is the last move in the typelist, i.e., the move to start with
  {
    p_nhe->FirstMove(st, mv.move);
    mv.selected = true; // but only the last neighborhood in the list is active
  }
  else
  {
    try
    {
      other_nhes->FirstMove(st, mv.movelist);   
    }
    catch (EmptyNeighborhood e)
    {
      p_nhe->FirstMove(st, mv.move);
      mv.selected = true;
    }
  }
}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
bool SetUnionNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::NextMove(const State& st, MoveList& mv) const
{
  bool exists_next_move;
  if (!mv.selected) // this neighborhood was not active yet
  {
    exists_next_move = other_nhes->NextMove(st, mv.movelist);
    if (exists_next_move)
      return true;    
    // all the following neighborhoods have already finished
    try
    {
      p_nhe->FirstMove(st, mv.move); 
    }
    catch (EmptyNeighborhood e)
    {
      mv.selected = false; // also this assignment is superfluous
      return false;
    }
    mv.selected = true;
    return true;
  } 
  // the neighborhood has already become active before
  exists_next_move = p_nhe->NextMove(st, mv.move);
  if (!exists_next_move)
  {
    mv.selected = false;
    return false;
  }
  else
    return true;
}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
void SetUnionNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::MakeMove(State& st, const MoveList& mv) const
{
  if (mv.selected)
    p_nhe->MakeMove(st, mv.move);
  else
    other_nhes->MakeMove(st, mv.movelist);
}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
CFtype SetUnionNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::DeltaCostFunction(const State& st, const MoveList& mv) const
{
  if (mv.selected)
    return p_nhe->DeltaCostFunction(st, mv.move);
  else
    return other_nhes->DeltaCostFunction(st, mv.movelist);
}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
ShiftedResult<CFtype> SetUnionNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::DeltaShiftedCostFunction(const State& st, const MoveList& mv) const
{
  if (mv.selected)
    return p_nhe->DeltaShiftedCostFunction(st, mv.move);
  else
    return other_nhes->DeltaShiftedCostFunction(st, mv.movelist);
}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
void SetUnionNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::SetAllUnselected(MoveList& mv) const
{
  mv.selected = false;
  other_nhes->SetAllUnselected(mv.movelist);
}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
unsigned int SetUnionNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::Modality() const
{
  return MoveList::length;
}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
unsigned int SetUnionNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::MoveModality(const MoveList& mv) const
{
  if (mv.selected)
    return MoveList::length - 1;
  else 
    return other_nhes->MoveModality(mv.movelist);
}

/** Template specialization for the end of the typelist (i.e., NullType) */

template <typename Input, typename State, typename CFtype>
class SetUnionNeighborhoodExplorer<Input, State, CFtype, NullType>
{
public:
  typedef NullType MoveList;
  
  SetUnionNeighborhoodExplorer(const Input& i,
                               StateManager<Input,State,CFtype>& e_sm, 
                               std::string e_name = "NullSetUnionNeighborhoodExplorer"); 
  SetUnionNeighborhoodExplorer(const Input& i,
                               StateManager<Input,State,CFtype>& e_sm, 
                               const std::vector<double>& bias,
                               std::string e_name = "NullSetUnionNeighborhoodExplorer"); 
  SetUnionNeighborhoodExplorer(const Input& i,
                               StateManager<Input,State,CFtype>& e_sm, 
                               const std::vector<double>& bias,
                               unsigned int index,
                               std::string e_name = "NullSetUnionNeighborhoodExplorer"); 
  template <typename NeighborhoodExplorer>
  void AddNeighborhoodExplorer(NeighborhoodExplorer& ne)
  {
    throw std::logic_error("Error passing a neighborhod explorer object to a Multimodal neighborhood explorer:"
                           "either the added neighborhood explorer is not of a compatible type or a compatible one has already been added");
  }
  void RandomMove(const State &st, NullType& mv) const;
  void RandomMove(const State &st, MoveList& mv, double random_value) const;  
  void FirstMove(const State& st, MoveList& mv) const;
  bool NextMove(const State& st, MoveList& mv) const;
  void MakeMove(State& st, const MoveList& mv) const;
  CFtype DeltaCostFunction(const State& st, const MoveList& mv) const;
  ShiftedResult<CFtype> DeltaShiftedCostFunction(const State& st, const MoveList& mv) const;
  void SetAllUnselected(MoveList& mv) const;
  unsigned int Modality() const;
  unsigned int MoveModality(const MoveList& mv) const;
protected:  
  const Input& in;/**< A reference to the input object */
  StateManager<Input, State, CFtype>& sm; /**< A reference to the attached state manager. */
  
  std::string name;
};

template <typename Input, typename State, typename CFtype>
SetUnionNeighborhoodExplorer<Input,State,CFtype,NullType>::SetUnionNeighborhoodExplorer(const Input& i,
                                                                                        StateManager<Input,State,CFtype>& e_sm, 
                                                                                        std::string e_name)
: in(i), sm(e_sm), name(e_name)
{}

template <typename Input, typename State, typename CFtype>
SetUnionNeighborhoodExplorer<Input,State,CFtype,NullType>::SetUnionNeighborhoodExplorer(const Input& i,
                                                                                        StateManager<Input,State,CFtype>& e_sm, 
                                                                                        const std::vector<double>& bias,
                                                                                        std::string e_name)
: in(i), sm(e_sm), name(e_name)
{}

template <typename Input, typename State, typename CFtype>
SetUnionNeighborhoodExplorer<Input,State,CFtype,NullType>::SetUnionNeighborhoodExplorer(const Input& i,
                                                                                        StateManager<Input,State,CFtype>& e_sm, 
                                                                                        const std::vector<double>& bias,
                                                                                        unsigned int index,
                                                                                        std::string e_name)
: in(i), sm(e_sm), name(e_name)
{}


template <typename Input, typename State, typename CFtype>
void SetUnionNeighborhoodExplorer<Input,State,CFtype,NullType>::RandomMove(const State &st, NullType& mv) const
{ throw EmptyNeighborhood(); }

template <typename Input, typename State, typename CFtype>
void SetUnionNeighborhoodExplorer<Input,State,CFtype,NullType>::RandomMove(const State &st, NullType& mv, double random_value) const
{ throw EmptyNeighborhood(); }

template <typename Input, typename State, typename CFtype>
void SetUnionNeighborhoodExplorer<Input,State,CFtype,NullType>::FirstMove(const State& st, MoveList& mv) const
{ throw EmptyNeighborhood(); }

template <typename Input, typename State, typename CFtype>
bool SetUnionNeighborhoodExplorer<Input,State,CFtype,NullType>::NextMove(const State& st, MoveList& mv) const
{ return false; }


// TODO: it is questionable whether this function could or could not be reached (doing nothing), see also the following methods
template <typename Input, typename State, typename CFtype>
void SetUnionNeighborhoodExplorer<Input,State,CFtype,NullType>::MakeMove(State& st, const MoveList& mv) const
{}

template <typename Input, typename State, typename CFtype>
CFtype SetUnionNeighborhoodExplorer<Input,State,CFtype,NullType>::DeltaCostFunction(const State& st, const MoveList& mv) const
{
  throw std::logic_error("Error: this function should never be reached through the typelist");
  return (CFtype)0; // just to prevent warnings
}

template <typename Input, typename State, typename CFtype>
ShiftedResult<CFtype> SetUnionNeighborhoodExplorer<Input,State,CFtype,NullType>::DeltaShiftedCostFunction(const State& st, const MoveList& mv) const
{
  throw std::logic_error("Error: this function should never be reached through the typelist");
  ShiftedResult<CFtype> sr;
  return sr; // just to prevent warnings
}

template <typename Input, typename State, typename CFtype>
void SetUnionNeighborhoodExplorer<Input,State,CFtype,NullType>::SetAllUnselected(MoveList& mv) const
{}

template <typename Input, typename State, typename CFtype>
unsigned int SetUnionNeighborhoodExplorer<Input,State,CFtype,NullType>::Modality() const
{
  throw std::logic_error("Error: this function should never be reached through the typelist");
  return 0;
}

template <typename Input, typename State, typename CFtype>
unsigned int SetUnionNeighborhoodExplorer<Input,State,CFtype,NullType>::MoveModality(const MoveList& mv) const
{
  throw std::logic_error("Error: this function should never be reached through the typelist");
  return 0;
}

// CartesianProduct

template <class Input, class State, typename CFtype, class NeighborhoodExplorerList>
CartesianProductNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::CartesianProductNeighborhoodExplorer(const Input& i, 
                                                                                                                        StateManager<Input,State,CFtype>& e_sm, 
                                                                                                                        std::string e_name)
: in(i), sm(e_sm),  p_nhe(NULL), other_nhes(i, e_sm, e_name), name(e_name)
{}

template <class Input, class State, typename CFtype, class NeighborhoodExplorerList>
CartesianProductNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::CartesianProductNeighborhoodExplorer(const Input& i, 
                                                                                                                        StateManager<Input,State,CFtype>& e_sm, 
                                                                                                                        const std::vector<double>& bias,
                                                                                                                        std::string e_name)
: CartesianProductNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>(i, e_sm, e_name)
{}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
void CartesianProductNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::RandomMove(const State &st, MoveList& mv) const
{
  p_nhe->RandomMove(st, mv.move);
  mv.selected = true;
  State st1 = st;
  p_nhe->MakeMove(st1, mv.move);
  other_nhes.RandomMove(st1, mv.movelist);
}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
void CartesianProductNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::FirstMove(const State& st, MoveList& mv) const
{
  p_nhe->FirstMove(st, mv.move);
  mv.selected = true;
  if (mv.length == 1)
    return;
  State st1 = st;
  p_nhe->MakeMove(st1, mv.move);
  bool exists_first_move;
  try
  {
    other_nhes.FirstMove(st1, mv.movelist);
    exists_first_move = true;
  }
  catch (EmptyNeighborhood e) {
    exists_first_move = false;
  }
  while (!exists_first_move) 
  {
    if (!p_nhe->NextMove(st, mv.move))
      throw EmptyNeighborhood();
    st1 = st;
    p_nhe->MakeMove(st1, mv.move);
    try
    {
      other_nhes.FirstMove(st1, mv.movelist);
      exists_first_move = true;
    }
    catch (EmptyNeighborhood e) {
      exists_first_move = false;
    }
  }
}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
bool CartesianProductNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::NextMove(const State& st, MoveList& mv) const
{
  if (mv.length == 1) // end of recursion
    return p_nhe->NextMove(st, mv.move);
  State st1 = st;
  p_nhe->MakeMove(st1, mv.move);
  if (other_nhes.NextMove(st1, mv.movelist))
    return true;
  bool exists_next_move;
  do
  {
    if (!p_nhe->NextMove(st, mv.move))
      return false;
    st1 = st;
    p_nhe->MakeMove(st1, mv.move);
    try 
    {
      other_nhes.FirstMove(st1, mv.movelist);
      exists_next_move = true;
    } catch (EmptyNeighborhood e)
    {
      exists_next_move = false;
    }
  } 
  while (!exists_next_move);
  return true;
}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
void CartesianProductNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::MakeMove(State& st, const MoveList& mv) const
{
  if (mv.length == 1) // end of recursion
    p_nhe->MakeMove(st, mv.move);
  else 
  {
    p_nhe->MakeMove(st, mv.move);
    other_nhes.MakeMove(st, mv.movelist);
  }
}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
CFtype CartesianProductNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::DeltaCostFunction(const State& st, const MoveList& mv) const
{
  if (mv.length == 1)
    return p_nhe->DeltaCostFunction(st, mv.move);
  State st1 = st;
  p_nhe->MakeMove(st1, mv.move);
  return other_nhes.DeltaCostFunction(st1, mv.movelist) + p_nhe->DeltaCostFunction(st, mv.move);
}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
ShiftedResult<CFtype> CartesianProductNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::DeltaShiftedCostFunction(const State& st, const MoveList& mv) const
{
  if (mv.length == 1)
    return p_nhe->DeltaShiftedCostFunction(st, mv.move);
  State st1 = st;
  p_nhe->MakeMove(st1, mv.move);
  ShiftedResult<CFtype> acc_value = other_nhes.DeltaShiftedCostFunction(st1, mv.movelist), cur_value = p_nhe->DeltaShiftedCostFunction(st, mv.move);
  acc_value.shifted_value += cur_value.shifted_value;
  acc_value.actual_value += cur_value.actual_value;
  return acc_value;
}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
unsigned int CartesianProductNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::Modality() const
{
  return 1;
}

template <typename Input, typename State, typename CFtype, class NeighborhoodExplorerList>
unsigned int CartesianProductNeighborhoodExplorer<Input,State,CFtype,NeighborhoodExplorerList>::MoveModality(const MoveList& mv) const
{
  return 0;
}

/** Template specialization for the end of the typelist (i.e., NullType) */

template <typename Input, typename State, typename CFtype>
class CartesianProductNeighborhoodExplorer<Input, State, CFtype, NullType>
{
public:
  typedef NullType MoveList;
  
  CartesianProductNeighborhoodExplorer(const Input& i,
                               StateManager<Input,State,CFtype>& e_sm, 
                               std::string e_name = "NullCartesianProductNeighborhoodExplorer"); 
  template <typename NeighborhoodExplorer>
  void AddNeighborhoodExplorer(NeighborhoodExplorer& ne)
  {
    throw std::logic_error("Error passing a neighborhod explorer object to a Multimodal neighborhood explorer:"
                           "either the added neighborhood explorer is not of a compatible type or a compatible one has already been added");
  }
  void RandomMove(const State &st, NullType& mv) const;
  void FirstMove(const State& st, MoveList& mv) const;
  bool NextMove(const State& st, MoveList& mv) const;
  void MakeMove(State& st, const MoveList& mv) const;
  CFtype DeltaCostFunction(const State& st, const MoveList& mv) const;
  ShiftedResult<CFtype> DeltaShiftedCostFunction(const State& st, const MoveList& mv) const;
  unsigned int Modality() const;
  unsigned int MoveModality(const MoveList& mv) const;
protected:
  
  const Input& in;/**< A reference to the input object */
  StateManager<Input, State, CFtype>& sm; /**< A reference to the attached state manager. */
  
  std::string name;
};

template <typename Input, typename State, typename CFtype>
CartesianProductNeighborhoodExplorer<Input,State,CFtype,NullType>::CartesianProductNeighborhoodExplorer(const Input& i,
                                                                                        StateManager<Input,State,CFtype>& e_sm, 
                                                                                        std::string e_name)
: in(i), sm(e_sm), name(e_name)
{}

template <typename Input, typename State, typename CFtype>
void CartesianProductNeighborhoodExplorer<Input,State,CFtype,NullType>::RandomMove(const State &st, NullType& mv) const
{ throw EmptyNeighborhood(); }

template <typename Input, typename State, typename CFtype>
void CartesianProductNeighborhoodExplorer<Input,State,CFtype,NullType>::FirstMove(const State& st, MoveList& mv) const
{ throw EmptyNeighborhood(); }

template <typename Input, typename State, typename CFtype>
bool CartesianProductNeighborhoodExplorer<Input,State,CFtype,NullType>::NextMove(const State& st, MoveList& mv) const
{ return false; }


// TODO: it is questionable whether this function could or could not be reached (doing nothing), see also the following methods
template <typename Input, typename State, typename CFtype>
void CartesianProductNeighborhoodExplorer<Input,State,CFtype,NullType>::MakeMove(State& st, const MoveList& mv) const
{}

template <typename Input, typename State, typename CFtype>
CFtype CartesianProductNeighborhoodExplorer<Input,State,CFtype,NullType>::DeltaCostFunction(const State& st, const MoveList& mv) const
{
  throw std::logic_error("Error: this function should never be reached through the typelist");
  return (CFtype)0; // just to prevent warnings
}

template <typename Input, typename State, typename CFtype>
ShiftedResult<CFtype> CartesianProductNeighborhoodExplorer<Input,State,CFtype,NullType>::DeltaShiftedCostFunction(const State& st, const MoveList& mv) const
{
  throw std::logic_error("Error: this function should never be reached through the typelist");
  ShiftedResult<CFtype> sr;
  return sr; // just to prevent warnings
}

template <typename Input, typename State, typename CFtype>
unsigned int CartesianProductNeighborhoodExplorer<Input,State,CFtype,NullType>::Modality() const
{
  throw std::logic_error("Error: this function should never be reached through the typelist");
  return 1;
}

template <typename Input, typename State, typename CFtype>
unsigned int CartesianProductNeighborhoodExplorer<Input,State,CFtype,NullType>::MoveModality(const MoveList& mv) const
{
  throw std::logic_error("Error: this function should never be reached through the typelist");
  return 0;
}

#endif // define _MULTIMODALNEIGHBORHOOD_EXPLORER_HH_
