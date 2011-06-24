// $Id: TabuListManager.hh 203 2008-05-21 16:42:02Z schaerf $
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2011 Andrea Schaerf, Luca Di Gaspero. 
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

#if !defined(_MULTIMODAL_TABU_LIST_MANAGER_HH_)
#define _MULTIMODAL_TABU_LIST_MANAGER_HH_

#include <helpers/TabuListManager.hh>
#include <helpers/MultimodalNeighborhoodExplorer.hh>

/** The SetUnion Tabu List Manager manages the tabu list prohibition 
 mechanism for the SetUnion multimodal neighborhood explorer.
 It can be used directly in a Runner through an adapter, which adjust
 a SetUnion Neighborhood Explorer in order to fit it within the usual 
 Runner hierarchy.
 
 @ingroup Helpers
 */

template <class State, typename CFtype, class TabuListManagerList>
class SetUnionTabuListManager 
{
protected:
  typedef typename TabuListManagerList::Head ThisTabuListManager;
  typedef SetUnionTabuListManager<State, CFtype, typename TabuListManagerList::Tail> OtherTabuListManager;
public:       
  typedef Movelist<CFtype, typename ThisTabuListManager::ThisMove, typename OtherTabuListManager::MoveList> MoveList;
  
  SetUnionTabuListManager();
  /**
   Adds a (monomodal) tabu list manager to the pool.
   @param tl the tabu list manager to be added
   */
  template <typename TabuListManager>
  void AddTabuListManager(TabuListManager& tl)
  {
    if (inspect_types<TabuListManager, ThisTabuListManager>::are_equal() && p_tlm == NULL) // the second condition is just to allow duplicated types in the typelist
      p_tlm = dynamic_cast<ThisTabuListManager*>(&tl); // only to prevent a compilation error
    else
      other_tlms.AddTabuListManager(tl);
  }
  
  bool Inverse(const MoveList& mv1, const MoveList& mv2) const;
protected:
  ThisTabuListManager* p_tlm;
  OtherTabuListManager other_tlms;
};

template <class State, typename CFtype, class TabuListManagerList>
class CartesianProductTabuListManager 
{
protected:
  typedef typename TabuListManagerList::Head ThisTabuListManager;
  typedef SetUnionTabuListManager<State, CFtype, class TabuListManagerList::Tail> OtherTabuListManager;
public:       
  typedef Movelist<CFtype, typename ThisTabuListManager::ThisMove, typename OtherTabuListManager::MoveList> MoveList;
  
  CartesianProductTabuListManager();
  /**
   Adds a (monomodal) tabu list manager to the pool.
   @param tl the tabu list manager to be added
   */
  template <typename TabuListManager>
  void AddTabuListManager(TabuListManager& tl)
  {
    if (inspect_types<TabuListManager, ThisTabuListManager>::are_equal() && p_tlm == NULL) // the second condition is just to allow duplicated types in the typelist
      p_tlm = dynamic_cast<ThisTabuListManager*>(&tl); // only to prevent a compilation error
    else
      other_tlms.AddTabuListManager(tl);
  }
  
  bool Inverse(const MoveList& mv1, const MoveList& mv2) const;
protected:
  ThisTabuListManager* p_tlm;
  OtherTabuListManager other_tlms;
};


template <class State, class MoveList, class MultimodalTabuListManager, typename CFtype = int>
class MultimodalTabuListManagerAdapter : public TabuListManager<State, MoveList, CFtype>
{
public:
  MultimodalTabuListManager mmtlm;
  MultimodalTabuListManagerAdapter(unsigned int min_tenure, unsigned int max_tenure)
  : TabuListManager<State, MoveList, CFtype>(min_tenure, max_tenure) {}
  MultimodalTabuListManagerAdapter()
  : TabuListManager<State, MoveList, CFtype>() {}

  template <typename TabuListManager>
  void AddTabuListManager(TabuListManager& tl) 
  {
    mmtlm.AddTabuListManager(tl);
  }

  bool Inverse(const MoveList& mv1, const MoveList& mv2) const
  { 
    return mmtlm.Inverse(mv1, mv2);
  }

};

template <typename State, typename TabuListManagerList, typename CFtype = int>
class PrepareSetUnionTabuListManager
{
public:
  typedef SetUnionTabuListManager<State, CFtype, TabuListManagerList> MultimodaTabuListManagerType;  
  typedef typename MultimodaTabuListManagerType::MoveList MoveList;
  typedef MultimodalTabuListManagerAdapter<State, MoveList, MultimodaTabuListManagerType, CFtype> TabuListManager;
};

template <typename State, typename TabuListManagerList, typename CFtype = int>
class PrepareCartesianProductTabuListManager
{
public:
typedef CartesianProductTabuListManager<State, CFtype, TabuListManagerList> MultimodaTabuListManagerType;  
typedef typename MultimodaTabuListManagerType::MoveList MoveList;
typedef MultimodalTabuListManagerAdapter<State, MoveList, MultimodaTabuListManagerType, CFtype> TabuListManager;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class State, typename CFtype, class TabuListManagerList>
SetUnionTabuListManager<State,CFtype,TabuListManagerList>::SetUnionTabuListManager()
: p_tlm(NULL)
{}

template <class State, typename CFtype, class TabuListManagerList>
bool SetUnionTabuListManager<State,CFtype,TabuListManagerList>::Inverse(const MoveList& mv1, const MoveList& mv2) const
{
  if (mv1.selected != mv2.selected)
    return false;
  if (mv1.selected && mv2.selected)
    return p_tlm->Inverse(mv1.move, mv2.move);
  return other_tlms.Inverse(mv1.movelist, mv2.movelist);    
}

/** Template specialization for the end of the typelist (i.e., NullType) */

template <class State, typename CFtype>
class SetUnionTabuListManager<State, CFtype, NullType>
{
public:  
  typedef NullType MoveList;
  
  template <typename TabuListManager>
  void AddTabuListManager(TabuListManager& tl)
  {
    throw std::logic_error("Error passing a tabu list manager object to a Multimodal tabu list manager:"
                           "either the added tabu list manager is not of a compatible type or a compatible one has already been added");
  }
  bool Inverse(const MoveList& mv1, const MoveList& mv2) const
  { return false; }
};

template <class State, typename CFtype, class TabuListManagerList>
CartesianProductTabuListManager<State,CFtype,TabuListManagerList>::CartesianProductTabuListManager()
: p_tlm(NULL)
{}

template <class State, typename CFtype, class TabuListManagerList>
bool CartesianProductTabuListManager<State,CFtype,TabuListManagerList>::Inverse(const MoveList& mv1, const MoveList& mv2) const
{
  return other_tlms.Inverse(mv1.movelist, mv2.movelist) && p_tlm->Inverse(mv1.move, mv2.move);
}

/** Template specialization for the end of the typelist (i.e., NullType) */

template <class State, typename CFtype>
class CartesianProductTabuListManager<State, CFtype, NullType>
{
public:  
  typedef NullType MoveList;
  
  template <typename TabuListManager>
  void AddTabuListManager(TabuListManager& tl)
  {
    throw std::logic_error("Error passing a tabu list manager object to a Multimodal tabu list manager:"
                           "either the added tabu list manager is not of a compatible type or a compatible one has already been added");
  }
  bool Inverse(const MoveList& mv1, const MoveList& mv2) const
  { return true; }

};


#endif // define _MULTIMODAL_TABU_LIST_MANAGER_HH_
